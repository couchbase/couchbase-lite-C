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
#include "CBLDocument_Internal.hh"
#include "CBLPrivate.h"
#include "Internal.hh"
#include "Util.hh"
#include "function_ref.hh"
#include "PlatformCompat.hh"
#include <sys/stat.h>

#ifndef CMAKE
#include <unistd.h>
#endif

using namespace std;
using namespace fleece;
using namespace cbl_internal;


#pragma mark - CONFIGURATION:


// Default location for databases. This is platform-dependent.
// (The implementation for Apple platforms is in CBLDatabase+ObjC.mm)
#ifndef __APPLE__
std::string CBLDatabase::defaultDirectory() {
    return cbl_getcwd(nullptr, 0);
}
#endif


static inline slice effectiveDir(slice inDirectory) {
    if (inDirectory) {
        return inDirectory;
    } else {
        static const string kDir = CBLDatabase::defaultDirectory();
        return slice(kDir);
    }
}

static_assert(sizeof(CBLEncryptionKey::bytes) == sizeof(C4EncryptionKey::bytes),
              "C4EncryptionKey and CBLEncryptionKey size do not match");

static C4EncryptionKey asC4Key(const CBLEncryptionKey *key) {
    C4EncryptionKey c4key;
    if (key) {
        c4key.algorithm = static_cast<C4EncryptionAlgorithm>(key->algorithm);
        memcpy(c4key.bytes, key->bytes, sizeof(CBLEncryptionKey::bytes));
    } else {
        c4key.algorithm = kC4EncryptionNone;
    }
    return c4key;
}

static C4DatabaseConfig2 asC4Config(const CBLDatabaseConfiguration *config) {
    CBLDatabaseConfiguration defaultConfig;
    if (!config) {
        defaultConfig = CBLDatabaseConfiguration_Default();
        config = &defaultConfig;
    }
    C4DatabaseConfig2 c4Config = {};
    c4Config.parentDirectory = effectiveDir(config->directory);
    c4Config.flags = kC4DB_Create | kC4DB_VersionVectors;
    c4Config.encryptionKey = asC4Key(config->encryptionKey);
    return c4Config;
}


#ifdef COUCHBASE_ENTERPRISE
bool CBLEncryptionKey_FromPassword(CBLEncryptionKey *key, FLString password) CBLAPI {
    C4EncryptionKey c4key;
    if (&c4key->setPassword(password, kC4EncryptionAES256)) {
        key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
        memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
        return true;
    } else {
        key->algorithm = kCBLEncryptionNone;
        return false;
    }
}
#endif


CBLDatabaseConfiguration CBLDatabaseConfiguration_Default() CBLAPI {
    CBLDatabaseConfiguration config = {};
    config.directory = effectiveDir(nullslice);
    return config;
}


#pragma mark - STATIC "METHODS":


bool CBL_DatabaseExists(FLString name, FLString inDirectory) CBLAPI {
    return C4Database::exists(name, effectiveDir(inDirectory));
}


bool CBL_CopyDatabase(FLString fromPath,
                      FLString toName,
                      const CBLDatabaseConfiguration* config,
                      CBLError* outError) CBLAPI
{
    C4DatabaseConfig2 c4config = asC4Config(config);
    C4Database::copyNamed(fromPath, toName, c4config);
}


bool CBL_DeleteDatabase(FLString name,
                        FLString inDirectory,
                        CBLError *outError) CBLAPI
{
    C4Database::deleteNamed(name, effectiveDir(inDirectory));
}


#pragma mark - LIFECYCLE & OPERATIONS:


CBLDatabase* CBLDatabase_Open(FLString name,
                              const CBLDatabaseConfiguration *config,
                              CBLError *outError) CBLAPI
{
    C4DatabaseConfig2 c4config = asC4Config(config);
    C4Database *c4db = C4Database::openNamed(name, &c4config, internal(outError));
    if (!c4db)
        return nullptr;
    if (c4db->mayHaveExpiration())
        c4db->startHousekeeping();
    return make_retained<CBLDatabase>(c4db, name, c4config.parentDirectory).detach();
}


bool CBLDatabase_Close(CBLDatabase* db, CBLError* outError) CBLAPI {
    if (!db)
        return true;
    return db->use<bool>([=](C4Database *c4db) {
        return c4db->close(internal(outError));
    });
}

bool CBLDatabase_BeginTransaction(CBLDatabase* db, CBLError* outError) CBLAPI {
    return db->use<bool>([=](C4Database *c4db) {
        return c4db->beginTransaction(internal(outError));
    });
}

bool CBLDatabase_EndTransaction(CBLDatabase* db, bool commit, CBLError* outError) CBLAPI {
    return db->use<bool>([=](C4Database *c4db) {
        return c4db->endTransaction(commit, internal(outError));
    });
}

bool CBLDatabase_Delete(CBLDatabase* db, CBLError* outError) CBLAPI {
    return db->use<bool>([=](C4Database *c4db) {
        return c4db->delete(internal(outError));
    });
}

#ifdef COUCHBASE_ENTERPRISE
bool CBLDatabase_ChangeEncryptionKey(CBLDatabase *db,
                                     const CBLEncryptionKey *newKey,
                                     CBLError* outError) CBLAPI
{
    return db->use<bool>([=](C4Database *c4db) {
        C4EncryptionKey c4key = asC4Key(newKey);
        return c4db->rekey(&c4key, internal(outError));
    });
}
#endif

bool CBLDatabase_PerformMaintenance(CBLDatabase* db,
                                    CBLMaintenanceType type,
                                    CBLError* outError) CBLAPI
{
    return db->use<bool>([=](C4Database *c4db) {
        return c4db->maintenance((C4MaintenanceType)type, internal(outError));
    });
}


#pragma mark - ACCESSORS:


// For use only by CBLURLEndpointListener and CBLLocalEndpoint
C4Database* CBLDatabase::_getC4Database() const {
    return use<C4Database*>([](C4Database *c4db) {
        return c4db;
    });
}


FLString CBLDatabase_Name(const CBLDatabase* db) CBLAPI {
    return db->use<FLString>([](C4Database *c4db) {
        return c4db->getName();
    });
}

FLStringResult CBLDatabase_Path(const CBLDatabase* db) CBLAPI {
    return db->use<FLStringResult>([](C4Database *c4db) {
        return c4db->getPath();
    });
}

const CBLDatabaseConfiguration CBLDatabase_Config(const CBLDatabase* db) CBLAPI {
    return {db->dir};
}

uint64_t CBLDatabase_Count(const CBLDatabase* db) CBLAPI {
    return db->use<uint64_t>([](C4Database *c4db) {
        return c4db->getDocumentCount();
    });
}

uint64_t CBLDatabase_LastSequence(const CBLDatabase* db) CBLAPI {
    return db->use<uint64_t>([](C4Database *c4db) {
        return c4db->getLastSequence();
    });
}


#pragma mark - DOCUMENTS:


Retained<CBLDocument> CBLDatabase::_getDocument(slice docID, bool isMutable, bool allRevisions,
                                                CBLError *outError)
{
    C4Document *c4doc = use<C4Document*>([&](C4Database *c4db) {
        return c4db->getDoc(docID, true, // mustExist
                           (allRevisions ? kDocGetAll : kDocGetCurrentRev),
                           internal(outError));
    });
    if (!c4doc) {
        if (outError && outError->code == CBLErrorNotFound && outError->domain == CBLDomain)
            *outError = {};             // not-found is not treated as an error.
        return nullptr;
    } else if (!allRevisions && (c4doc->flags() & kDocDeleted)) {
        c4doc->release();
        if (outError)
            *outError = {};             // not-found is not treated as an error.
        return nullptr;
    } else {
        return make_retained<CBLDocument>(docID, this, c4doc, isMutable);
    }
}


RetainedConst<CBLDocument>
CBLDatabase::getDocument(slice docID, bool allRevisions, CBLError *outError) const {
    return const_cast<CBLDatabase*>(this)->_getDocument(docID, false, allRevisions, outError);
}


Retained<CBLDocument>
CBLDatabase::getMutableDocument(slice docID, CBLError* outError) {
    return this->_getDocument(docID, true, true, outError);
}


const CBLDocument* CBLDatabase_GetDocument(const CBLDatabase* db, FLString docID, CBLError* outError) CBLAPI {
    return db->getDocument(docID, false, outError).detach();
}


CBLDocument* CBLDatabase_GetMutableDocument(CBLDatabase* db, FLString docID, CBLError* outError) CBLAPI {
    return db->getMutableDocument(docID, outError).detach();
}


#pragma mark - NOTIFICATIONS:


void CBLDatabase_BufferNotifications(CBLDatabase *db,
                                     CBLNotificationsReadyCallback callback,
                                     void *context) CBLAPI
{
    db->bufferNotifications(callback, context);
}

void CBLDatabase_SendNotifications(CBLDatabase *db) CBLAPI {
    db->sendNotifications();
}


#pragma mark - DATABASE CHANGE LISTENERS:


Retained<CBLListenerToken> CBLDatabase::addListener(function_ref<Retained<CBLListenerToken>()> callback) {
    return use<Retained<CBLListenerToken>>([=](C4Database *c4db) {
        Retained<CBLListenerToken> token = callback();
        if (!_observer) {
            _observer = c4db,
                                       [](C4DatabaseObserver* observer, void *context) {
                ((CBLDatabase*)context)->databaseChanged();
            },
                                       this->create();
        }
        return token;
    });
}


Retained<CBLListenerToken> CBLDatabase::addListener(CBLDatabaseChangeListener listener, void *ctx) {
    return addListener([&]{ return _listeners.add(listener, ctx); });
}


Retained<CBLListenerToken> CBLDatabase::addListener(CBLDatabaseChangeDetailListener listener, void *ctx) {
    return addListener([&]{ return _detailListeners.add(listener, ctx); });
}


void CBLDatabase::databaseChanged() {
    notify(bind(&CBLDatabase::callDBListeners, this));
}


void CBLDatabase::callDBListeners() {
    static const uint32_t kMaxChanges = 100;
    while (true) {
        C4DatabaseChange c4changes[kMaxChanges];
        bool external;
        uint32_t nChanges = _observer->getChanges(c4changes, kMaxChanges, &external);
        if (nChanges == 0)
            break;

        static_assert(sizeof(CBLDatabaseChange) == sizeof(C4DatabaseChange));
        _detailListeners.call(this, nChanges, (const CBLDatabaseChange*)c4changes);

        if (!_listeners.empty()) {
            FLString docIDs[kMaxChanges];
            for (uint32_t i = 0; i < nChanges; ++i)
                docIDs[i] = c4changes[i].docID;
            _listeners.call(this, nChanges, docIDs);
        }
    }
}


CBLListenerToken* CBLDatabase_AddChangeListener(const CBLDatabase* constdb,
                                                CBLDatabaseChangeListener listener,
                                                void *context) CBLAPI
{
    return const_cast<CBLDatabase*>(constdb)->addListener(listener, context).detach();
}


CBLListenerToken* CBLDatabase_AddChangeDetailListener(const CBLDatabase* constdb,
                                                      CBLDatabaseChangeDetailListener listener,
                                                      void *context) CBLAPI
{
    return const_cast<CBLDatabase*>(constdb)->addListener(listener, context).detach();
}


#pragma mark - DOCUMENT LISTENERS:


namespace cbl_internal {

    // Custom subclass of CBLListenerToken for document listeners.
    // (It implements the ListenerToken<> template so that it will work with Listeners<>.)
    template<>
    struct ListenerToken<CBLDocumentChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLDatabase *db, slice docID, CBLDocumentChangeListener callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_db(db)
        ,_docID(docID)
        {
            db->use([&](C4Database *c4db) {
                _c4obs = c4db,
                                         docID,
                                         [](C4DocumentObserver* observer, C4String docID,
                                            C4SequenceNumber sequence, void *context)
                                         {
                                             ((ListenerToken*)context)->docChanged();
                                         },
                                         this->create();
            });
        }

        ~ListenerToken() {
            _db->use([&](C4Database *c4db) {
                _c4obs->free();
            });
        }

        CBLDocumentChangeListener callback() const {
            return (CBLDocumentChangeListener)_callback.load();
        }

        // this is called indirectly by CBLDatabase::sendNotifications
        void call(const CBLDatabase*, FLString) {
            auto cb = callback();
            if (cb)
                cb(_context, _db, _docID);
        }

    private:
        void docChanged() {
            _db->notify(this, _db, _docID);
        }

        CBLDatabase* _db;
        alloc_slice _docID;
        C4DocumentObserver* _c4obs {nullptr};
    };

}


Retained<CBLListenerToken> CBLDatabase::addDocListener(slice docID,
                                                       CBLDocumentChangeListener listener,
                                                       void *context)
{
    auto token = new ListenerToken<CBLDocumentChangeListener>(this, docID, listener, context);
    _docListeners.add(token);
    return token;
}


CBLListenerToken* CBLDatabase_AddDocumentChangeListener(const CBLDatabase* db _cbl_nonnull,
                                             FLString docID,
                                             CBLDocumentChangeListener listener _cbl_nonnull,
                                             void *context) CBLAPI
{
    return const_cast<CBLDatabase*>(db)->addDocListener(docID, listener, context).detach();

}
