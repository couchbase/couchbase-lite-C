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


static constexpr CBLDatabaseFlags kDefaultFlags = kC4DB_Create;


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

static C4DatabaseConfig2 asC4Config(const CBLDatabaseConfiguration_s *config) {
    CBLDatabaseConfiguration_s defaultConfig;
    if (!config) {
        defaultConfig = CBLDatabaseConfiguration_Default_s();
        config = &defaultConfig;
    }
    C4DatabaseConfig2 c4Config = {};
    c4Config.parentDirectory = effectiveDir(config->directory);
    if (config->flags & kCBLDatabase_Create)
        c4Config.flags |= kC4DB_Create;
    if (config->flags & kCBLDatabase_ReadOnly)
        c4Config.flags |= kC4DB_ReadOnly;
    if (config->flags & kCBLDatabase_NoUpgrade)
        c4Config.flags |= kC4DB_NoUpgrade;
    if (config->encryptionKey) {
        c4Config.encryptionKey.algorithm = static_cast<C4EncryptionAlgorithm>(
                                                            config->encryptionKey->algorithm);
        static_assert(sizeof(CBLEncryptionKey::bytes) == sizeof(C4EncryptionKey::bytes),
                      "C4EncryptionKey and CBLEncryptionKey size do not match");
        memcpy(c4Config.encryptionKey.bytes, config->encryptionKey->bytes,
               sizeof(CBLEncryptionKey::bytes));
    } else {
        c4Config.encryptionKey.algorithm = kC4EncryptionNone;
    }
    return c4Config;
}


CBLDatabaseConfiguration CBLDatabaseConfiguration_Default() {
    static const string kDir = CBLDatabase::defaultDirectory();
    CBLDatabaseConfiguration config = {};
    config.directory = kDir.c_str();
    config.flags = kDefaultFlags;
    return config;
}


CBLDatabaseConfiguration_s CBLDatabaseConfiguration_Default_s() {
    CBLDatabaseConfiguration_s config = {};
    config.directory = effectiveDir(nullslice);
    config.flags = kDefaultFlags;
    return config;
}


#pragma mark - STATIC "METHODS":


bool CBL_DatabaseExists(const char *name, const char *inDirectory) CBLAPI {
    return CBL_DatabaseExists_s(slice(name), slice(inDirectory));
}

bool CBL_DatabaseExists_s(FLString name, FLString inDirectory) CBLAPI {
    return c4db_exists(name, effectiveDir(inDirectory));
}


bool CBL_CopyDatabase(const char* fromPath,
                      const char* toName,
                      const CBLDatabaseConfiguration *config,
                      CBLError* outError) CBLAPI
{
    CBLDatabaseConfiguration_s config_s = {}, *configP = nullptr;
    if (config) {
        config_s = {slice(config->directory), config->flags, config->encryptionKey};
        configP = &config_s;
    }
    return CBL_CopyDatabase_s(slice(fromPath), slice(toName), configP, outError);
}

bool CBL_CopyDatabase_s(FLString fromPath,
                        FLString toName,
                        const CBLDatabaseConfiguration_s* config,
                        CBLError* outError) CBLAPI
{
    C4DatabaseConfig2 c4config = asC4Config(config);
    return c4db_copyNamed(fromPath, toName, &c4config, internal(outError));
}


bool CBL_DeleteDatabase(const char *name,
                        const char *inDirectory,
                        CBLError* outError) CBLAPI
{
    return CBL_DeleteDatabase_s(slice(name), slice(inDirectory), outError);
}

bool CBL_DeleteDatabase_s(FLString name,
                          FLString inDirectory,
                          CBLError *outError) CBLAPI
{
    return c4db_deleteNamed(name, effectiveDir(inDirectory), internal(outError));
}


#pragma mark - LIFECYCLE & OPERATIONS:


CBLDatabase* CBLDatabase_Open(const char *name,
                              const CBLDatabaseConfiguration *config,
                              CBLError *outError) CBLAPI
{
    CBLDatabaseConfiguration_s config_s = {}, *configP = nullptr;
    if (config) {
        config_s = {slice(config->directory), config->flags, config->encryptionKey};
        configP = &config_s;
    }
    return CBLDatabase_Open_s(slice(name), configP, outError);
}

CBLDatabase* CBLDatabase_Open_s(FLString name,
                                const CBLDatabaseConfiguration_s *config,
                                CBLError *outError) CBLAPI
{
    C4DatabaseConfig2 c4config = asC4Config(config);
    C4Database *c4db = c4db_openNamed(slice(name), &c4config, internal(outError));
    if (!c4db)
        return nullptr;
    if (c4db_mayHaveExpiration(c4db))
        c4db_startHousekeeping(c4db);
    return retain(new CBLDatabase(c4db, name,
                                  c4config.parentDirectory,
                                  (config ? config->flags : kDefaultFlags)));
}


bool CBLDatabase_Close(CBLDatabase* db, CBLError* outError) CBLAPI {
    if (!db)
        return true;
    return db->use<bool>([=](C4Database *c4db) {
        return c4db_close(c4db, internal(outError));
    });
}

bool CBLDatabase_BeginBatch(CBLDatabase* db, CBLError* outError) CBLAPI {
    return db->use<bool>([=](C4Database *c4db) {
        return c4db_beginTransaction(c4db, internal(outError));
    });
}

bool CBLDatabase_EndBatch(CBLDatabase* db, CBLError* outError) CBLAPI {
    return db->use<bool>([=](C4Database *c4db) {
        return c4db_endTransaction(c4db, true, internal(outError));
    });
}

bool CBLDatabase_Delete(CBLDatabase* db, CBLError* outError) CBLAPI {
    return db->use<bool>([=](C4Database *c4db) {
        return c4db_delete(c4db, internal(outError));
    });
}

#ifdef COUCHBASE_ENTERPRISE
bool CBLDatabase_Rekey(CBLDatabase* db, const CBLEncryptionKey *newKey, CBLError* outError) CBLAPI {
    return db->use<bool>([=](C4Database *c4db) {
        return c4db_rekey(c4db, (const C4EncryptionKey*)newKey, internal(outError));
    });
}
#endif

bool CBLDatabase_PerformMaintenance(CBLDatabase* db,
                                    CBLMaintenanceType type,
                                    CBLError* outError) CBLAPI
{
    return db->use<bool>([=](C4Database *c4db) {
        return c4db_maintenance(c4db, (C4MaintenanceType)type, internal(outError));
    });
}


#pragma mark - ACCESSORS:


// For use only by CBLURLEndpointListener and CBLLocalEndpoint
C4Database* CBLDatabase::_getC4Database() const {
    return use<C4Database*>([](C4Database *c4db) {
        return c4db;
    });
}


const char* CBLDatabase_Name(const CBLDatabase* db) CBLAPI {
    return db->name.c_str();
}

const char* CBLDatabase_Path(const CBLDatabase* db) CBLAPI {
    return db->path.c_str();
}

const CBLDatabaseConfiguration CBLDatabase_Config(const CBLDatabase* db) CBLAPI {
    const char *dir = db->dir.empty() ? nullptr : db->dir.c_str();
    return {dir, db->flags};
}

uint64_t CBLDatabase_Count(const CBLDatabase* db) CBLAPI {
    return db->use<uint64_t>([](C4Database *c4db) {
        return c4db_getDocumentCount(c4db);
    });
}

uint64_t CBLDatabase_LastSequence(const CBLDatabase* db) CBLAPI {
    return db->use<uint64_t>([](C4Database *c4db) {
        return c4db_getLastSequence(c4db);
    });
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


Retained<CBLListenerToken> CBLDatabase::addListener(function_ref<CBLListenerToken*()> callback) {
    return use<Retained<CBLListenerToken>>([=](C4Database *c4db) {
        Retained<CBLListenerToken> token = callback();
        if (!_observer) {
            _observer = c4dbobs_create(c4db,
                                       [](C4DatabaseObserver* observer, void *context) {
                ((CBLDatabase*)context)->databaseChanged();
            },
                                       this);
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
        uint32_t nChanges = c4dbobs_getChanges(_observer, c4changes, kMaxChanges, &external);
        if (nChanges == 0)
            break;

        static_assert(sizeof(CBLDatabaseChange) == sizeof(C4DatabaseChange));
        _detailListeners.call(this, nChanges, (const CBLDatabaseChange*)c4changes);

        if (!_listeners.empty()) {
            // Convert docID slices to C strings:
            const char* docIDs[kMaxChanges];
            size_t bufSize = 0;
            for (uint32_t i = 0; i < nChanges; ++i)
                bufSize += c4changes[i].docID.size + 1;
            char *buf = new char[bufSize], *next = buf;
            for (uint32_t i = 0; i < nChanges; ++i) {
                docIDs[i] = next;
                memcpy(next, (const char*)c4changes[i].docID.buf, c4changes[i].docID.size);
                next += c4changes[i].docID.size;
                *(next++) = '\0';
            }
            assert(next - buf == bufSize);
            // Call the listener(s):
            _listeners.call(this, nChanges, docIDs);
            delete [] buf;
        }
        c4dbobs_releaseChanges(c4changes, nChanges);
    }
}


CBLListenerToken* CBLDatabase_AddChangeListener(const CBLDatabase* constdb,
                                                CBLDatabaseChangeListener listener,
                                                void *context) CBLAPI
{
    return retain(const_cast<CBLDatabase*>(constdb)->addListener(listener, context));
}


CBLListenerToken* CBLDatabase_AddChangeDetailListener(const CBLDatabase* constdb,
                                                      CBLDatabaseChangeDetailListener listener,
                                                      void *context) CBLAPI
{
    return retain(const_cast<CBLDatabase*>(constdb)->addListener(listener, context));
}


#pragma mark - DOCUMENT LISTENERS:


namespace cbl_internal {

    // Custom subclass of CBLListenerToken for document listeners.
    // (It implements the ListenerToken<> template so that it will work with Listeners<>.)
    template<>
    class ListenerToken<CBLDocumentChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLDatabase *db, const char *docID, CBLDocumentChangeListener callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_db(db)
        ,_docID(docID)
        {
            db->use([&](C4Database *c4db) {
                _c4obs = c4docobs_create(c4db,
                                         slice(docID),
                                         [](C4DocumentObserver* observer, C4String docID,
                                            C4SequenceNumber sequence, void *context)
                                         {
                                             ((ListenerToken*)context)->docChanged();
                                         },
                                         this);
            });
        }

        ~ListenerToken() {
            _db->use([&](C4Database *c4db) {
                c4docobs_free(_c4obs);
            });
        }

        CBLDocumentChangeListener callback() const {
            return (CBLDocumentChangeListener)_callback.load();
        }

        // this is called indirectly by CBLDatabase::sendNotifications
        void call(const CBLDatabase*, const char*) {
            auto cb = callback();
            if (cb)
                cb(_context, _db, _docID.c_str());
        }

    private:
        void docChanged() {
            _db->notify(this, nullptr, nullptr);
        }

        CBLDatabase* _db;
        string _docID;
        C4DocumentObserver* _c4obs {nullptr};
    };

}


Retained<CBLListenerToken> CBLDatabase::addDocListener(const char* docID _cbl_nonnull,
                                                       CBLDocumentChangeListener listener, void *context)
{
    auto token = new ListenerToken<CBLDocumentChangeListener>(this, docID, listener, context);
    _docListeners.add(token);
    return token;
}


CBLListenerToken* CBLDatabase_AddDocumentChangeListener(const CBLDatabase* db _cbl_nonnull,
                                             const char* docID _cbl_nonnull,
                                             CBLDocumentChangeListener listener _cbl_nonnull,
                                             void *context) CBLAPI
{
    return retain(const_cast<CBLDatabase*>(db)->addDocListener(docID, listener, context));

}
