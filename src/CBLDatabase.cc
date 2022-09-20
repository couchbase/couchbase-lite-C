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
    try {
        auto c4key = C4EncryptionKeyFromPassword(password, kC4EncryptionAES256);
        key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
        memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
        return true;
    } catchAndWarn();
}

bool CBLEncryptionKey_FromPasswordOld(CBLEncryptionKey *key, FLString password) CBLAPI {
    try {
        auto c4key = C4EncryptionKeyFromPasswordSHA1(password, kC4EncryptionAES256);
        key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
        memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
        return true;
    } catchAndWarn();
}
#endif


// The methods below cannot be in C4Database_Internal.hh because they depend on
// CBLCollection_Internal.hh, which would create a circular header dependency.


#pragma mark - CONSTRUCTORS:


CBLDatabase::CBLDatabase(C4Database* _cbl_nonnull db, slice name_, slice dir_)
:_dir(dir_)
,_name(name_)
,_notificationQueue(this)
{
    _c4db = std::make_shared<C4DatabaseAccessLock>(db);
    _defaultCollection = getCollection(kC4DefaultCollectionName, kC4DefaultScopeID);
}


CBLDatabase::~CBLDatabase() {
    _c4db->useLockedIgnoredWhenClosed([&](Retained<C4Database> &c4db) {
        _closed();
    });
}


#pragma mark - LIFE CYCLE:


void CBLDatabase::close() {
    stopActiveStoppables();
    
    try {
        auto db = _c4db->useLocked();
        db->close();
        _closed();
    } catch (litecore::error& e) {
        if (e == litecore::error::NotOpen) {
            return;
        }
        throw;
    }
}


void CBLDatabase::closeAndDelete() {
    stopActiveStoppables();
    
    auto db = _c4db->useLocked();
    db->closeAndDeleteFile();
    _closed();
}


/** Must called under _c4db lock. */
void CBLDatabase::_closed() {
    // Close scopes:
    for (auto& i : _scopes) {
        i.second->close();
    }
    // Close collections:
    for (auto& i : _collections) {
        i.second->close();
    }
    // Close the access lock:
    _c4db->close();
}


#pragma mark - SCOPES:


Retained<CBLScope> CBLDatabase::getScope(slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db->useLocked();
    
    CBLScope* scope = nullptr;
    
    bool exist = c4db->hasScope(scopeName);
    if (auto i = _scopes.find(scopeName); i != _scopes.end()) {
        if (!exist) {
            // Detach instead of close so that the retained scope
            // object can retain the database and can be valid to use.
            i->second->detach();
            
            // Remove from the _scopes map.
            _scopes.erase(i);
            
            // Remove all collections in the scope from the _collections map:
            // This is techically required to prevent circular reference
            // (db->collection->scope->db) when the scope is detached.
            removeCBLCollections(scopeName);
            return nullptr;
        }
        scope = i->second.get();
    }
    
    if (!scope && exist) {
        auto retainedScope = make_retained<CBLScope>(scopeName, this);
        scope = retainedScope.get();
        _scopes.insert({scope->name(), move(retainedScope)});
    }
    
    return scope;
}


#pragma mark - COLLECTIONS:


Retained<CBLCollection> CBLDatabase::getCollection(slice collectionName, slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db->useLocked();
    
    CBLCollection* collection = nullptr;
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    if (auto i = _collections.find(spec); i != _collections.end()) {
        collection = i->second.get();
    }
    
    if (collection && collection->isValid()) {
        return collection;
    }
    
    auto c4col = c4db->getCollection(spec);
    if (!c4col) {
        if (collection) {
            removeCBLCollection(spec); // Invalidate cache
        }
        return nullptr;
    }
    
    auto scope = getScope(scopeName);
    if (!scope) {
        // Note (Edge Case):
        // The scope is NULL because at the same time, its all collections including the
        // the one just created were deleted on a different thread using another database.
        return nullptr;
    }
    
    return createCBLCollection(c4col, scope.get());
}


Retained<CBLCollection> CBLDatabase::createCollection(slice collectionName, slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db->useLocked();
    
    auto col = getCollection(collectionName, scopeName);
    if (col) {
        return col;
    }
    
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    auto c4col = c4db->createCollection(spec);
    
    bool cache = true;
    auto scope = getScope(scopeName);
    if (!scope) {
        // Note (Edge Case):
        // The scope is NULL because at the same time, its all collections including the
        // the one just created were deleted on a different thread using another database.
        // So create the scope in non-cached mode, and the returned collection will not
        // be cached.
        cache = false;
        scope = new CBLScope(scopeName, this, cache);
    }
    return createCBLCollection(c4col, scope.get(), cache);
}


bool CBLDatabase::deleteCollection(slice collectionName, slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db->useLocked();
    
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    c4db->deleteCollection(spec);
    removeCBLCollection(spec);
    return true;
}


Retained<CBLScope> CBLDatabase::getDefaultScope() {
    _c4db->useLocked();
    return getScope(kC4DefaultScopeID);
}


Retained<CBLCollection> CBLDatabase::getDefaultCollection(bool mustExist) {
    auto db = _c4db->useLocked();
    
    if (_defaultCollection && !_defaultCollection->isValid()) {
        _defaultCollection = nullptr;
    }
    
    if (!_defaultCollection && mustExist) {
        C4Error::raise(LiteCoreDomain, kC4ErrorNotOpen,
                       "Invalid collection: either deleted, or db closed");
    }
    
    return _defaultCollection;
}


Retained<CBLCollection> CBLDatabase::createCBLCollection(C4Collection* c4col, CBLScope* scope, bool cache) {
    auto retainedCollection = make_retained<CBLCollection>(c4col, scope, const_cast<CBLDatabase*>(this));
    auto collection = retainedCollection.get();
    if (cache) {
        _collections.insert({C4Database::CollectionSpec(c4col->getSpec()), move(retainedCollection)});
    }
    return collection;
}


void CBLDatabase::removeCBLCollection(C4Database::CollectionSpec spec) {
    if (auto i = _collections.find(spec); i != _collections.end()) {
        i->second.get()->close();
        _collections.erase(i);
    }
}

void CBLDatabase::removeCBLCollections(slice scopeName) {
    auto i = _collections.begin();
    while (i != _collections.end()) {
        if (i->first.scope == scopeName) {
            i->second->close();
            i = _collections.erase(i);
        } else {
            i++;
        }
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
    auto c4query = _c4db->useLocked()->newQuery((C4QueryLanguage)language, queryString, outErrPos);
    if (!c4query)
        return nullptr;
    return new CBLQuery(this, std::move(c4query), *_c4db);
}


namespace cbl_internal {

    void ListenerToken<CBLQueryChangeListener>::queryChanged() {
        _query->database()->notify(this);
    }

}


#pragma mark - BINDING DEV SUPPORT FOR BLOB


Retained<CBLBlob> CBLDatabase::getBlob(FLDict properties) {
    auto c4db = _c4db->useLocked();
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
    auto c4db = _c4db->useLocked();
    blob->install(this);
}
