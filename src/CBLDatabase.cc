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
    // Close the access lock:
    _c4db->close();
}


#pragma mark - SCOPES:


Retained<CBLScope> CBLDatabase::getScope(slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db->useLocked();
    
    bool exist = c4db->hasScope(scopeName);
    if (!exist) {
        return nullptr;
    }
    return new CBLScope(scopeName, this);
}


#pragma mark - COLLECTIONS:


Retained<CBLCollection> CBLDatabase::getCollection(slice collectionName, slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db->useLocked();
    
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    auto c4col = c4db->getCollection(spec);
    if (!c4col) {
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
    
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    auto c4col = c4db->createCollection(spec);
    
    auto scope = new CBLScope(scopeName, this);
    return createCBLCollection(c4col, scope);
}


bool CBLDatabase::deleteCollection(slice collectionName, slice scopeName) {
    if (!scopeName)
        scopeName = kC4DefaultScopeID;
    
    auto c4db = _c4db->useLocked();
    
    auto spec = C4Database::CollectionSpec(collectionName, scopeName);
    c4db->deleteCollection(spec);
    return true;
}


Retained<CBLScope> CBLDatabase::getDefaultScope() {
    return getScope(kC4DefaultScopeID);
}


Retained<CBLCollection> CBLDatabase::getDefaultCollection() {
    return getCollection(kC4DefaultCollectionName, kC4DefaultScopeID);
}


Retained<CBLCollection> CBLDatabase::createCBLCollection(C4Collection* c4col, CBLScope* scope) {
    return new CBLCollection(c4col, scope, const_cast<CBLDatabase*>(this));
}


Retained<CBLCollection> CBLDatabase::getInternalDefaultCollection() {
    if (!_defaultCollection) {
        _defaultCollection = getCollection(kC4DefaultCollectionName, kC4DefaultScopeID);
        _defaultCollection->adopt(this); // Prevent the retain cycle
    }
    return _defaultCollection;
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
