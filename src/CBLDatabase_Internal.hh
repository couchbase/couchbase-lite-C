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
#include "CBLPrivate.h"
#include "CBLQuery_Internal.hh"
#include "c4Database.hh"
#include "c4Observer.hh"
#include "Internal.hh"
#include "Listener.hh"
#include "access_lock.hh"
#include "function_ref.hh"
#include "fleece/Mutable.hh"
#include <string>
#include <utility>

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
                             const CBLDatabaseConfiguration *config)
    {
        C4DatabaseConfig2 c4config = asC4Config(config);
        C4Database::copyNamed(fromPath, toName, c4config);
    }

    static void deleteDatabase(slice name, slice inDirectory) {
        C4Database::deleteNamed(name, effectiveDir(inDirectory));
    }

    static Retained<CBLDatabase> open(slice name, const CBLDatabaseConfiguration *config =nullptr) {
        C4DatabaseConfig2 c4config = asC4Config(config);
        Retained<C4Database> c4db = C4Database::openNamed(name, c4config);
        if (c4db->mayHaveExpiration())
            c4db->startHousekeeping();
        return new CBLDatabase(c4db, name, c4config.parentDirectory);
    }

    void performMaintenance(CBLMaintenanceType type) {
        _c4db.useLocked()->maintenance((C4MaintenanceType)type);
    }

#ifdef COUCHBASE_ENTERPRISE
    void changeEncryptionKey(const CBLEncryptionKey *newKey) {
        C4EncryptionKey c4key = asC4Key(newKey);
        _c4db.useLocked()->rekey(&c4key);
    }
#endif

    void beginTransaction()                          {_c4db.useLocked()->beginTransaction();}
    void endTransaction(bool commit)                 {_c4db.useLocked()->endTransaction(commit);}

    void close()                                     {_c4db.useLocked()->close();}
    void closeAndDelete()                            {_c4db.useLocked()->closeAndDeleteFile();}


#pragma mark - Accessors:


    slice name() const noexcept                      {return _c4db.useLocked()->getName();}
    alloc_slice path() const                         {return _c4db.useLocked()->path();}
    CBLDatabaseConfiguration config() const noexcept {return {_dir, nullptr};}
    C4BlobStore* blobStore() const                   {return _blobStore;}

    uint64_t count() const                           {return _c4db.useLocked()->getDocumentCount();}
    uint64_t lastSequence() const                    {return _c4db.useLocked()->getLastSequence();}


#pragma mark - Documents:


    RetainedConst<CBLDocument> getDocument(slice docID, bool allRevisions =false) const {
        return _getDocument(docID, false, allRevisions);
    }

    Retained<CBLDocument> getMutableDocument(slice docID) {
        return _getDocument(docID, true, true);
    }

    bool deleteDocument(const CBLDocument *doc _cbl_nonnull,
                        CBLConcurrencyControl concurrency)
    {
        CBLDocument::SaveOptions opt(concurrency);
        opt.deleting = true;
        return const_cast<CBLDocument*>(doc)->save(this, opt);
    }

    bool deleteDocument(slice docID) {
        auto c4db = _c4db.useLocked();
        C4Database::Transaction t(c4db);
        Retained<C4Document> c4doc = c4db->getDocument(docID, false, kDocGetCurrentRev);
        if (c4doc)
            c4doc = c4doc->update(nullslice, kRevDeleted);
        if (!c4doc)
            return false;
        t.commit();
        return true;
    }

    bool purgeDocument(slice docID) {
        return _c4db.useLocked()->purgeDoc(docID);
    }

    CBLTimestamp getDocumentExpiration(slice docID) {
        return _c4db.useLocked()->getExpiration(docID);
    }

    void setDocumentExpiration(slice docID, CBLTimestamp expiration) {
        auto c4db = _c4db.useLocked();
        c4db->setExpiration(docID, expiration);
        if (expiration > 0)
            c4db->startHousekeeping();
    }


#pragma mark - Queries & Indexes:


    Retained<CBLQuery> createQuery(CBLQueryLanguage language,
                                   slice queryString,
                                   int *outErrPos) const
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

    void createIndex(slice name, CBLIndexSpec spec) {
        C4IndexOptions options = {};
        options.ignoreDiacritics = spec.ignoreAccents;
        std::string languageStr;
        if (spec.language.buf) {
            languageStr = std::string(spec.language);
            options.language = languageStr.c_str();
        }
        _c4db.useLocked()->createIndex(name,
                                 spec.keyExpressionsJSON,
                                 (C4IndexType)spec.type,
                                 &options);
    }

    void deleteIndex(slice name) {
        _c4db.useLocked()->deleteIndex(name);
    }

    fleece::MutableArray indexNames() {
        Doc doc(_c4db.useLocked()->getIndexesInfo());
        auto indexes = fleece::MutableArray::newArray();
        for (Array::iterator i(doc.root().asArray()); i; ++i) {
            Dict info = i.value().asDict();
            indexes.append(info["name"]);
        }
        return indexes;
    }


#pragma mark - Listeners:


    Retained<CBLListenerToken> addListener(CBLDatabaseChangeListener _cbl_nonnull listener, void *ctx) {
        return addListener([&]{ return _listeners.add(listener, ctx); });
    }

    Retained<CBLListenerToken> addListener(CBLDatabaseChangeDetailListener _cbl_nonnull listener,
                                           void *ctx) {
        return addListener([&]{ return _detailListeners.add(listener, ctx); });
    }

    Retained<CBLListenerToken> addDocListener(slice docID,
                                              CBLDocumentChangeListener _cbl_nonnull,
                                              void *context);

    void sendNotifications() {
        _notificationQueue.notifyAll();
    }

    void bufferNotifications(CBLNotificationsReadyCallback callback, void *context) {
        _notificationQueue.setCallback(callback, context);
    }


#pragma mark - Internals:


protected:
    friend class CBLDocument;
    friend class CBLReplicator;
    friend struct CBLURLEndpointListener;
    friend class cbl_internal::AllConflictsResolver;
    friend class cbl_internal::CBLLocalEndpoint;
    friend class cbl_internal::ListenerToken<CBLDocumentChangeListener>;
    friend class cbl_internal::ListenerToken<CBLQueryChangeListener>;

    template <class LISTENER, class... Args>
    void notify(ListenerToken<LISTENER> *listener, Args... args) const {
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
    CBLDatabase(C4Database* _cbl_nonnull db, slice name_, slice dir_)
    :_c4db(std::move(db))
    ,_dir(dir_)
    ,_blobStore(&db->getBlobStore())
    ,_notificationQueue(this)
    { }

    virtual ~CBLDatabase() {
        _c4db.useLocked([&](Retained<C4Database> &c4db) {
            _docListeners.clear();
            _observer = nullptr;
        });
    }

    // Default location for databases. This is platform-dependent.
    static std::string defaultDirectory();

#ifdef COUCHBASE_ENTERPRISE
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
#endif

    static C4DatabaseConfig2 asC4Config(const CBLDatabaseConfiguration *config) {
        CBLDatabaseConfiguration defaultConfig;
        if (!config) {
            defaultConfig = CBLDatabaseConfiguration_Default();
            config = &defaultConfig;
        }
        C4DatabaseConfig2 c4Config = {};
        c4Config.parentDirectory = effectiveDir(config->directory);
        c4Config.flags = kC4DB_Create | kC4DB_VersionVectors;
#ifdef COUCHBASE_ENTERPRISE
        c4Config.encryptionKey = asC4Key(config->encryptionKey);
#endif
        return c4Config;
    }


    static slice effectiveDir(slice inDirectory) {
        if (inDirectory) {
            return inDirectory;
        } else {
            static const string kDir = defaultDirectory();
            return slice(kDir);
        }
    }

    Retained<CBLDocument> _getDocument(slice docID, bool isMutable, bool allRevisions) const {
        C4DocContentLevel content = (allRevisions ? kDocGetAll : kDocGetCurrentRev);
        Retained<C4Document> c4doc = _c4db.useLocked()->getDocument(docID, true, content);
        if (!c4doc || (!allRevisions && (c4doc->flags() & kDocDeleted)))
            return nullptr;
        return new CBLDocument(docID, const_cast<CBLDatabase*>(this), c4doc, isMutable);
    }

    Retained<CBLListenerToken> addListener(fleece::function_ref<Retained<CBLListenerToken>()> cb) {
        auto c4db = _c4db.useLocked(); // locks DB mutex, so the callback can run thread-safe
        Retained<CBLListenerToken> token = cb();
        if (!_observer)
            _observer = c4db->observe([this](C4DatabaseObserver*) { this->databaseChanged(); });
        return token;
    }

    void databaseChanged() {
        notify(bind(&CBLDatabase::callDBListeners, this));
    }

    void callDBListeners() {
        static const uint32_t kMaxChanges = 100;
        while (true) {
            C4DatabaseObserver::Change c4changes[kMaxChanges];
            bool external;
            uint32_t nChanges = _observer->getChanges(c4changes, kMaxChanges, &external);
            if (nChanges == 0)
                break;

            static_assert(sizeof(CBLDatabaseChange) == sizeof(C4DatabaseObserver::Change));
            _detailListeners.call(this, nChanges, (const CBLDatabaseChange*)c4changes);

            if (!_listeners.empty()) {
                FLString docIDs[kMaxChanges];
                for (uint32_t i = 0; i < nChanges; ++i)
                docIDs[i] = c4changes[i].docID;
                _listeners.call(this, nChanges, docIDs);
            }
        }
    }

    void callDocListeners();

    template <class T> using Listeners = cbl_internal::Listeners<T>;

    litecore::access_lock<Retained<C4Database>> _c4db;
    alloc_slice const                           _dir;
    C4BlobStore*                                _blobStore;
    std::unique_ptr<C4DatabaseObserver>         _observer;
    Listeners<CBLDatabaseChangeListener>        _listeners;
    Listeners<CBLDatabaseChangeDetailListener>  _detailListeners;
    Listeners<CBLDocumentChangeListener>        _docListeners;
    NotificationQueue                           _notificationQueue;
};
