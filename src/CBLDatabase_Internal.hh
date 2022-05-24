//
//  CBLDatabase_Internal.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

#pragma once
#include "CBLDatabase.h"
#include "CBLDocument_Internal.hh"
#include "CBLLog_Internal.hh"
#include "CBLPrivate.h"
#include "c4Collection.hh"
#include "c4Database.hh"
#include "c4Observer.hh"
#include "Error.hh"
#include "Internal.hh"
#include "Listener.hh"
#include "access_lock.hh"
#include "fleece/function_ref.hh"
#include "fleece/Mutable.hh"
#include <condition_variable>
#include <string>
#include <utility>
#include <unordered_set>

CBL_ASSUME_NONNULL_BEGIN

namespace cbl_internal {
    class AllConflictsResolver;
    struct CBLLocalEndpoint;
}


struct CBLDatabase final : public CBLRefCounted {
public:

#pragma mark - Lifecycle:

    static CBLDatabaseConfiguration defaultConfiguration() {
        CBLDatabaseConfiguration config = {};
        config.directory = effectiveDir(fleece::nullslice);
        return config;
    }

    static bool exists(slice name, slice inDirectory) {
        return C4Database::exists(name, effectiveDir(inDirectory));
    }

    static void copyDatabase(slice fromPath,
                             slice toName,
                             const CBLDatabaseConfiguration* _cbl_nullable config)
    {
        CBLLog_Init();
        C4DatabaseConfig2 c4config = asC4Config(config);
        C4Database::copyNamed(fromPath, toName, c4config);
    }

    static void deleteDatabase(slice name, slice inDirectory) {
        CBLLog_Init();
        C4Database::deleteNamed(name, effectiveDir(inDirectory));
    }

    static Retained<CBLDatabase> open(slice name,
                                      const CBLDatabaseConfiguration* _cbl_nullable config =nullptr)
    {
#ifdef __ANDROID__
        if (!getInitContext()) {
            C4Error::raise(LiteCoreDomain, kC4ErrorUnsupported,
                           "The context hasn't been initialized. Call CBL_Init(CBLInitContext*) to initialize the context");
        }
#endif
        CBLLog_Init();
        C4DatabaseConfig2 c4config = asC4Config(config);
        Retained<C4Database> c4db = C4Database::openNamed(name, c4config);
        return new CBLDatabase(c4db, name, c4config.parentDirectory);
    }

    void performMaintenance(CBLMaintenanceType type) {
        _c4db.useLocked()->maintenance((C4MaintenanceType)type);
    }

#ifdef COUCHBASE_ENTERPRISE
    void changeEncryptionKey(const CBLEncryptionKey* _cbl_nullable newKey) {
        C4EncryptionKey c4key = asC4Key(newKey);
        _c4db.useLocked()->rekey(&c4key);
    }
#endif

    void beginTransaction()                          {_c4db.useLocked()->beginTransaction();}
    void endTransaction(bool commit)                 {_c4db.useLocked()->endTransaction(commit);}
    
    void close() {
        stopActiveStoppables();
        _c4db.useLocked()->close();
    }
    
    void closeAndDelete() {
        stopActiveStoppables();
        _c4db.useLocked()->closeAndDeleteFile();
    }


#pragma mark - Accessors:


    slice name() const noexcept                      {return _c4db.useLocked()->getName();}
    alloc_slice path() const                         {return _c4db.useLocked()->getPath();}
    
    CBLDatabaseConfiguration config() const noexcept {
        auto &c4config = _c4db.useLocked()->getConfiguration();
#ifdef COUCHBASE_ENTERPRISE
        return {c4config.parentDirectory, asCBLKey(c4config.encryptionKey)};
#else
        return {c4config.parentDirectory};
#endif
    }

    uint64_t count() const                           {return _c4db.useLocked()->getDocumentCount();}
    uint64_t lastSequence() const                    {return static_cast<uint64_t>(_c4db.useLocked()->getLastSequence());}

    
#pragma mark - Collections:
    
    
    fleece::MutableArray scopeNames() const {
        auto names = FLMutableArray_New();
        _c4db.useLocked()->forEachScope([&](slice scope) {
            FLMutableArray_AppendString(names, scope);
        });
        return names;
    }
    
    fleece::MutableArray collectionNames(slice scopeName) const {
        auto names = FLMutableArray_New();
        _c4db.useLocked()->forEachCollection(scopeName, [&](C4CollectionSpec spec) {
            FLMutableArray_AppendString(names, spec.name);
        });
        return names;
    }
    
    CBLScope* _cbl_nullable getScope(slice scopeName);
    
    CBLCollection* _cbl_nullable getCollection(slice collectionName, slice scopeName) const;
    
    CBLCollection* createCollection(slice collectionName, slice scopeName);
    
    bool deleteCollection(slice collectionName, slice scopeName);
    
    CBLScope* getDefaultScope() {
        auto scope = getScope(kC4DefaultScopeID);
        assert(scope);
        return scope;
    }
    
    /**
     * Returned the default collection retained by the database. It will be used for any database's operations
     * that refer to the default collection. If the default collection doesn't exist when getting from the database,
     * the method will throw kC4ErrorNotOpen exception. */
    CBLCollection* _cbl_nullable getDefaultCollection(bool mustExist);
    

#pragma mark - Queries & Indexes:


    Retained<CBLQuery> createQuery(CBLQueryLanguage language,
                                   slice queryString,
                                   int* _cbl_nullable outErrPos) const;
    

#pragma mark - Listeners:
    

    void sendNotifications() {
        _notificationQueue.notifyAll();
    }

    void bufferNotifications(CBLNotificationsReadyCallback callback, void *context) {
        _notificationQueue.setCallback(callback, context);
    }


#pragma mark - Binding Dev Support for Blob:
    
    
    Retained<CBLBlob> getBlob(FLDict properties);
    
    void saveBlob(CBLBlob* blob);
    

#pragma mark - Internals:


protected:
    
    friend struct CBLBlob;
    friend struct CBLNewBlob;
    friend struct CBLBlobWriteStream;
    friend struct CBLCollection;
    friend struct CBLDocument;
    friend struct CBLReplicator;
    friend struct CBLURLEndpointListener;
    friend class cbl_internal::AllConflictsResolver;
    friend struct cbl_internal::CBLLocalEndpoint;
    friend struct cbl_internal::ListenerToken<CBLDocumentChangeListener>;
    friend struct cbl_internal::ListenerToken<CBLQueryChangeListener>;
    friend struct cbl_internal::ListenerToken<CBLCollectionDocumentChangeListener>;

    C4BlobStore* blobStore() const                   {return &_c4db.useLocked()->getBlobStore();}

    template <class LISTENER, class... Args>
    void notify(ListenerToken<LISTENER>* _cbl_nonnull listener, Args... args) const {
        Retained<ListenerToken<LISTENER>> retained = listener;
        notify([=]() {
            retained->call(args...);
        });
    }

    void notify(Notification n) const   {const_cast<CBLDatabase*>(this)->_notificationQueue.add(n);}

    auto useLocked()                  { return _c4db.useLocked(); }
    template <class LAMBDA>
    void useLocked(LAMBDA callback)   { _c4db.useLocked(callback); }
    template <class RESULT, class LAMBDA>
    RESULT useLocked(LAMBDA callback) { return _c4db.useLocked<RESULT>(callback); }

private:
    
    CBLDatabase(C4Database* _cbl_nonnull db, slice name_, slice dir_);

    virtual ~CBLDatabase();

    // Default location for databases. This is platform-dependent.
    static std::string defaultDirectory();

#ifdef COUCHBASE_ENTERPRISE
    static C4EncryptionKey asC4Key(const CBLEncryptionKey* _cbl_nullable key) {
        C4EncryptionKey c4key;
        if (key) {
            c4key.algorithm = static_cast<C4EncryptionAlgorithm>(key->algorithm);
            memcpy(c4key.bytes, key->bytes, sizeof(CBLEncryptionKey::bytes));
        } else {
            c4key.algorithm = kC4EncryptionNone;
        }
        return c4key;
    }

    static CBLEncryptionKey asCBLKey(const C4EncryptionKey &c4key) {
        CBLEncryptionKey key;
        key.algorithm = static_cast<CBLEncryptionAlgorithm>(c4key.algorithm);
        memcpy(key.bytes, c4key.bytes, sizeof(CBLEncryptionKey::bytes));
        return key;
    }
#endif

    static C4DatabaseConfig2 asC4Config(const CBLDatabaseConfiguration* _cbl_nullable config) {
        CBLDatabaseConfiguration defaultConfig;
        if (!config) {
            defaultConfig = CBLDatabaseConfiguration_Default();
            config = &defaultConfig;
        }
        C4DatabaseConfig2 c4Config = {};
        c4Config.parentDirectory = effectiveDir(config->directory);
        c4Config.flags = kC4DB_Create;
#ifdef COUCHBASE_ENTERPRISE
        c4Config.encryptionKey = asC4Key(&config->encryptionKey);
#endif
        return c4Config;
    }


    static slice effectiveDir(slice inDirectory) {
        if (inDirectory) {
            return inDirectory;
        } else {
            static const std::string kDir = defaultDirectory();
            return slice(kDir);
        }
    }

    /**
     Create a CBLCollection from the C4Collection.
     The created CBLCollection will be retained and cached in the _collections map. */
    CBLCollection* createCBLCollection(C4Collection* c4col) const;
    
    /** Remove and release the CBLCollection from the _collections map */
    void removeCBLCollection(C4Database::CollectionSpec spec) const;

    void callDocListeners();
    
    void stopActiveStoppables() {
        std::unordered_set<CBLStoppable*> stoppables;
        {
            std::lock_guard<std::mutex> lock(_stopMutex);
            if (_stopping)
                return;
            _stopping = true;
            stoppables = _stoppables;
        }
        
        // Call stop outside lock to prevent deadlock:
        for (auto s : stoppables) {
            s->stop();
        }
        
        std::unique_lock<std::mutex> lock(_stopMutex);
        if (!_stoppables.empty()) {
            CBL_Log(kCBLLogDomainDatabase, kCBLLogInfo,
                    "Waiting for %zu active replicators and live queries to stop ...",
                    _stoppables.size());
            _stopCond.wait(lock, [this] {return _stoppables.empty();});
        }
    }
    
    bool registerStoppable(CBLStoppable* stoppable) {
        LOCK(_stopMutex);
        if (_stopping)
            return false;
        _stoppables.insert(stoppable);
        return true;
    }
    
    void unregisterStoppable(CBLStoppable* stoppable) {
        LOCK(_stopMutex);
        _stoppables.erase(stoppable);
        _stopCond.notify_one();
    }

    template <class T> using Listeners = cbl_internal::Listeners<T>;

    using ScopesMap = std::unordered_map<slice, Retained<CBLScope>>;
    using CollectionsMap = std::unordered_map<C4Database::CollectionSpec, Retained<CBLCollection>>;
    
    litecore::access_lock<Retained<C4Database>> _c4db;
    alloc_slice const                           _dir;
    
    mutable ScopesMap                           _scopes;
    mutable CollectionsMap                      _collections;
    Retained<CBLCollection>                     _defaultCollection;
    
    // For sending notifications:
    NotificationQueue                           _notificationQueue;
    
    // For Active Stoppables:
    bool                                        _stopping {false};
    mutable std::mutex                          _stopMutex;
    std::condition_variable                     _stopCond;
    std::unordered_set<CBLStoppable*>           _stoppables;
};

CBL_ASSUME_NONNULL_END
