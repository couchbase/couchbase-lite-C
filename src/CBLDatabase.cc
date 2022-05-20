//
// CBLDatabase.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "CBLDatabase_Internal.hh"
#include "CBLBlob_Internal.hh"
#include "CBLCollection_Internal.hh"
#include "CBLQuery_Internal.hh"
#include "CBLPrivate.h"
#include "CBLScope_Internal.hh"
#include "c4Observer.hh"
#include "c4Query.hh"
#include "Internal.hh"
#include "fleece/function_ref.hh"
#include "fleece/PlatformCompat.hh"
#include <sys/stat.h>

#ifndef CMAKE
#include <unistd.h>
#endif

using namespace std;
using namespace fleece;
using namespace cbl_internal;


#pragma mark - CONFIGURATION:


// Default location for databases. This is platform-dependent.
// * Apple : CBLDatabase+Apple.mm
// * Android : CBLDatabase+Android.cc
#if !defined(__APPLE__) && !defined(__ANDROID__)
std::string CBLDatabase::defaultDirectory() {
    return cbl_getcwd(nullptr, 0);
}
#endif


#ifdef COUCHBASE_ENTERPRISE
static_assert(sizeof(CBLEncryptionKey::bytes) == sizeof(C4EncryptionKey::bytes),
              "C4EncryptionKey and CBLEncryptionKey size do not match");

bool CBLEncryptionKey_FromPassword(CBLEncryptionKey *key, FLString password) CBLAPI {
    auto c4key = C4EncryptionKeyFromPassword(password, kC4EncryptionAES256);    //FIXME: Catch
    key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
    memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
    return true;
}

bool CBLEncryptionKey_FromPasswordOld(CBLEncryptionKey *key, FLString password) CBLAPI {
    auto c4key = C4EncryptionKeyFromPasswordSHA1(password, kC4EncryptionAES256);    //FIXME: Catch
    key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
    memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
    return true;
}
#endif


// The methods below cannot be in C4Database_Internal.hh because they depend on
// CBLCollection_Internal.hh, which would create a circular header dependency.


#pragma mark - CONSTRUCTORS:


CBLDatabase::CBLDatabase(C4Database* _cbl_nonnull db, slice name_, slice dir_)
:_c4db(std::move(db))
,_dir(dir_)
,_notificationQueue(this)
{ }


CBLDatabase::~CBLDatabase() {
    _c4db.useLocked([&](Retained<C4Database> &c4db) {
        // Invalidate the database reference for both scopes and collections:
        for (auto& i : _scopes) {
            i.second->close();
        }
        
        for (auto& i : _collections) {
            i.second->close();
        }
    });
}


#pragma mark - SCOPES:


CBLScope* CBLDatabase::getScope(slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db.useLocked();
    
    // TODO: change to use hasScope()
    bool exist = (scopeName == kC4DefaultScopeID); // Default scope always exist.
    if (!exist) {
        c4db->forEachScope([&](slice scope) {
            if (!exist && scopeName == scope)
                exist = true;
        });
    }
    
    CBLScope* scope = nullptr;
    if (auto i = _scopes.find(scopeName); i != _scopes.end()) {
        if (exist) {
            scope = i->second.get();
        } else {
            _scopes.erase(i);
        }
    }
    
    if (exist && !scope) {
        auto retainedScope = make_retained<CBLScope>(scopeName, this);
        scope = retainedScope.get();
        _scopes.insert({scope->name(), move(retainedScope)});
    }
    return scope;
}


#pragma mark - COLLECTIONS:


CBLCollection* CBLDatabase::getCollection(slice collectionName, slice scopeName) const {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;

    auto c4db = _c4db.useLocked();
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    auto c4col = c4db->getCollection(spec);
    if (!c4col) {
        removeCBLCollection(spec);
        return nullptr;
    }
    return getOrCreateCBLCollection(c4col);
}


CBLCollection* CBLDatabase::createCollection(slice collectionName, slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db.useLocked();
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    auto c4col = c4db->createCollection(spec);
    return getOrCreateCBLCollection(c4col);
}


bool CBLDatabase::deleteCollection(slice collectionName, slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db.useLocked();
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    c4db->deleteCollection(spec);
    removeCBLCollection(spec);
    return true;
}


CBLCollection* CBLDatabase::getDefaultCollection() const {
    return getDefaultCollection(false);
}


CBLCollection* CBLDatabase::getDatabaseDefaultCollectionOrThrow() {
    auto c4db = _c4db.useLocked();
    if (!_defaultCollection)
        _defaultCollection = getDefaultCollection(true);
    return _defaultCollection;
}


CBLCollection* CBLDatabase::getDefaultCollection(bool mustExist) const {
    auto c4db = _c4db.useLocked();
    auto c4col = c4db->getDefaultCollection();
    if (!c4col) {
        auto spec = C4Database::CollectionSpec(kC4DefaultCollectionName, kC4DefaultScopeID);
        removeCBLCollection(spec);
        if (mustExist) {
            C4Error::raise(LiteCoreDomain, kC4ErrorNotOpen,
                       "Invalid collection: either deleted, or db closed");
        }
        return nullptr;
    }
    return getOrCreateCBLCollection(c4col);
}


CBLCollection* CBLDatabase::getOrCreateCBLCollection(C4Collection* c4col) const {
    CBLCollection* collection = nullptr;
    auto spec = C4Database::CollectionSpec(c4col->getSpec());
    if (auto i = _collections.find(spec); i != _collections.end()) {
        collection = i->second.get();
    }
    
    if (collection && collection->c4col() == c4col) {
        return collection;
    }
    
    auto retainedCollection = make_retained<CBLCollection>(c4col, const_cast<CBLDatabase*>(this));
    collection = retainedCollection.get();
    _collections.insert({spec, move(retainedCollection)});
    return collection;
}


void CBLDatabase::removeCBLCollection(C4Database::CollectionSpec spec) const {
    if (auto i = _collections.find(spec); i != _collections.end()) {
        _collections.erase(i);
    }
}


#pragma mark - QUERY:


Retained<CBLQuery> CBLDatabase::createQuery(CBLQueryLanguage language,
                                            slice queryString,
                                            int* _cbl_nullable outErrPos) const
{
    alloc_slice json;
    if (language == kCBLJSONLanguage) {
        json = convertJSON5(queryString); // allow JSON5 as a convenience
        queryString = json;
    }
    auto c4query = _c4db.useLocked()->newQuery((C4QueryLanguage)language, queryString, outErrPos);
    if (!c4query)
        return nullptr;
    return new CBLQuery(this, std::move(c4query), _c4db);
}


namespace cbl_internal {

    void ListenerToken<CBLQueryChangeListener>::queryChanged() {
        _query->database()->notify(this);
    }

}


#pragma mark - BINDING DEV SUPPORT FOR BLOB


Retained<CBLBlob> CBLDatabase::getBlob(FLDict properties) {
    try {
        return new CBLBlob(this, properties);
    } catch (...) {
        C4Error err = C4Error::fromCurrentException();
        if (err == C4Error{LiteCoreDomain, kC4ErrorNotFound})
            return nullptr;
        throw;
    }
}


void CBLDatabase::saveBlob(CBLBlob* blob) {
    blob->install(this);
}
