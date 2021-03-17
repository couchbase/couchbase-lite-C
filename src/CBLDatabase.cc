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
#include "c4Observer.hh"
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


slice CBLDatabase::effectiveDir(slice inDirectory) {
    if (inDirectory) {
        return inDirectory;
    } else {
        static const string kDir = defaultDirectory();
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

C4DatabaseConfig2 CBLDatabase::asC4Config(const CBLDatabaseConfiguration *config) {
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


CBLDatabaseConfiguration CBLDatabase::defaultConfiguration() {
    CBLDatabaseConfiguration config = {};
    config.directory = effectiveDir(nullslice);
    return config;
}


#ifdef COUCHBASE_ENTERPRISE
bool CBLEncryptionKey_FromPassword(CBLEncryptionKey *key, FLString password) CBLAPI {
    auto c4key = C4EncryptionKeyFromPassword(password, kC4EncryptionAES256);    //FIXME: Catch
    key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
    memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
    return true;
}
#endif


#pragma mark - STATIC METHODS:


bool CBLDatabase::exists(slice name, slice inDirectory) {
    return C4Database::exists(name, effectiveDir(inDirectory));
}


void CBLDatabase::copyDatabase(slice fromPath,
                               slice toName,
                               const CBLDatabaseConfiguration *config)
{
    C4DatabaseConfig2 c4config = asC4Config(config);
    C4Database::copyNamed(fromPath, toName, c4config);  //FIXME: Catch exceptions
}


void CBLDatabase::deleteDatabase(slice name, slice inDirectory) {
    C4Database::deleteNamed(name, effectiveDir(inDirectory));  //FIXME: Catch exceptions
}


#pragma mark - LIFECYCLE & OPERATIONS:


CBLDatabase::CBLDatabase(C4Database* _cbl_nonnull db, slice name_, slice dir_)
:access_lock(std::move(db))
,dir(dir_)
,_blobStore(&db->getBlobStore())
,_notificationQueue(this)
{ }


CBLDatabase::~CBLDatabase() {
    use([&](Retained<C4Database> &c4db) {
        _docListeners.clear();
        _observer = nullptr;
    });
}


Retained<CBLDatabase> CBLDatabase::open(slice name, const CBLDatabaseConfiguration *config) {
    C4DatabaseConfig2 c4config = asC4Config(config);
    Retained<C4Database> c4db = C4Database::openNamed(name, c4config);
    if (c4db->mayHaveExpiration())
        c4db->startHousekeeping();
    return new CBLDatabase(c4db, name, c4config.parentDirectory);
}


void CBLDatabase::close() {
    use([=](C4Database *c4db) {
        c4db->close();  //FIXME: Catch exceptions
    });
}

void CBLDatabase::beginTransaction() {
    use([=](C4Database *c4db) {
        c4db->beginTransaction();  //FIXME: Catch exceptions
    });
}

void CBLDatabase::endTransaction(bool commit) {
    use([=](C4Database *c4db) {
        c4db->endTransaction(commit);  //FIXME: Catch exceptions
    });
}

void CBLDatabase::closeAndDelete() {
    use([=](C4Database *c4db) {
        return c4db->closeAndDeleteFile();  //FIXME: Catch exceptions
    });
}

#ifdef COUCHBASE_ENTERPRISE
void CBLDatabase::changeEncryptionKey(const CBLEncryptionKey *newKey) {
    use([=](C4Database *c4db) {
        C4EncryptionKey c4key = asC4Key(newKey);
        c4db->rekey(&c4key);  //FIXME: Catch exceptions
    });
}
#endif

bool CBLDatabase::performMaintenance(CBLMaintenanceType type) {
    use([=](C4Database *c4db) {
        return c4db->maintenance((C4MaintenanceType)type);
    });
    return true;
}


#pragma mark - ACCESSORS:


// For use only by CBLURLEndpointListener and CBLLocalEndpoint
C4Database* CBLDatabase::_getC4Database() const {
    return use<C4Database*>([](C4Database *c4db) {
        return c4db;
    });
}


slice CBLDatabase::name() const noexcept {
    return use<FLString>([](C4Database *c4db) {
        return c4db->getName();
    });
}

alloc_slice CBLDatabase::path() const {
    return use<FLStringResult>([](C4Database *c4db) {
        return FLStringResult(c4db->path());
    });
}

CBLDatabaseConfiguration CBLDatabase::config() const noexcept {
    return {dir, nullptr};
}

uint64_t CBLDatabase::count() const {
    return use<uint64_t>([](C4Database *c4db) {
        return c4db->getDocumentCount();
    });
}

uint64_t CBLDatabase::lastSequence() const {
    return use<uint64_t>([](C4Database *c4db) {
        return c4db->getLastSequence();
    });
}


#pragma mark - DOCUMENTS:


Retained<CBLDocument> CBLDatabase::_getDocument(slice docID,
                                                bool isMutable,
                                                bool allRevisions) const
{
    Retained<C4Document> c4doc;
    use([&](const C4Database *c4db) {
        c4doc = c4db->getDocument(docID, true, // mustExist
                                 (allRevisions ? kDocGetAll : kDocGetCurrentRev));  //FIXME: Catch exceptions
    });
    if (!c4doc || (!allRevisions && (c4doc->flags() & kDocDeleted)))
        return nullptr;
    c4doc_retain(c4doc);//TEMP!!!
    return new CBLDocument(docID, const_cast<CBLDatabase*>(this), c4doc, isMutable);
}


RetainedConst<CBLDocument> CBLDatabase::getDocument(slice docID) const {
    return _getDocument(docID, false, false);
}

RetainedConst<CBLDocument> CBLDatabase::getDocument(slice docID, bool allRevisions) const {
    return _getDocument(docID, false, allRevisions);
}

Retained<CBLDocument> CBLDatabase::getMutableDocument(slice docID) {
    return _getDocument(docID, true, true);
}


#pragma mark - DATABASE CHANGE LISTENERS:


void CBLDatabase::sendNotifications() {
    _notificationQueue.notifyAll();
}

void CBLDatabase::bufferNotifications(CBLNotificationsReadyCallback callback, void *context) {
    _notificationQueue.setCallback(callback, context);
}


Retained<CBLListenerToken> CBLDatabase::addListener(function_ref<Retained<CBLListenerToken>()> callback) {
    return use<Retained<CBLListenerToken>>([=](C4Database *c4db) {
        Retained<CBLListenerToken> token = callback();
        if (!_observer)
            _observer = c4db->observe([this](C4DatabaseObserver*) { this->databaseChanged(); });
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
                _c4obs = c4db->observeDocument(docID,
                                         [this](C4DocumentObserver*, slice docID, C4SequenceNumber)
                                         {
                                             this->docChanged();
                                         });
            });
        }

        ~ListenerToken() {
            _db->use([&](C4Database *c4db) {
                _c4obs = nullptr;
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
        unique_ptr<C4DocumentObserver> _c4obs;
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
