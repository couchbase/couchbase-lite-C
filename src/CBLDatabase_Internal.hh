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
#include "CBLDocument.h"
#include "CBLPrivate.h"
#include "c4Database.hh"
#include "Internal.hh"
#include "Listener.hh"
#include "access_lock.hh"
#include "function_ref.hh"
#include <string>
#include <utility>

namespace cbl_internal {
    struct CBLLocalEndpoint;
}

struct CBLDatabase final : public CBLRefCounted, public litecore::access_lock<fleece::Retained<C4Database>> {
public:
    // Lifecycle:

    static CBLDatabaseConfiguration defaultConfiguration();

    static bool exists(slice name,
                       slice inDirectory);

    static void copyDatabase(slice fromPath,
                             slice toName,
                             const CBLDatabaseConfiguration *config);

    static void deleteDatabase(slice name,
                               slice inDirectory);

    static Retained<CBLDatabase> open(slice name,
                                      const CBLDatabaseConfiguration *config =nullptr);

    bool performMaintenance(CBLMaintenanceType);

#ifdef COUCHBASE_ENTERPRISE
    void changeEncryptionKey(const CBLEncryptionKey*);
#endif

    void beginTransaction();
    void endTransaction(bool commit);

    void close();
    void closeAndDelete();

    // Accessors:

    slice name() const noexcept;
    alloc_slice path() const;
    CBLDatabaseConfiguration config() const noexcept;
    C4BlobStore* blobStore() const      {return _blobStore;}

    uint64_t count() const;
    uint64_t lastSequence() const;

    // Documents:

    RetainedConst<CBLDocument> getDocument(slice docID) const ;

    Retained<CBLDocument> getMutableDocument(slice docID);

    // Listeners:

    Retained<CBLListenerToken> addListener(CBLDatabaseChangeListener _cbl_nonnull,
                                                   void *ctx);
    Retained<CBLListenerToken> addListener(CBLDatabaseChangeDetailListener _cbl_nonnull,
                                                   void *ctx);

    Retained<CBLListenerToken> addDocListener(slice docID,
                                              CBLDocumentChangeListener _cbl_nonnull,
                                              void *context);

    void sendNotifications();

    void bufferNotifications(CBLNotificationsReadyCallback callback, void *context);

//protected:
    RetainedConst<CBLDocument> getDocument(slice docID, bool allRevisions) const;

    template <class LISTENER, class... Args>
    void notify(ListenerToken<LISTENER> *listener, Args... args) const {
        Retained<ListenerToken<LISTENER>> retained = listener;
        notify([=]() {
            retained->call(args...);
        });
    }

    void notify(Notification n) const   {const_cast<CBLDatabase*>(this)->_notificationQueue.add(n);}

private:
    friend struct CBLURLEndpointListener;
    friend class cbl_internal::CBLLocalEndpoint;

    CBLDatabase(C4Database* _cbl_nonnull db, slice name_, slice dir_);
    virtual ~CBLDatabase();

    // Default location for databases. This is platform-dependent.
    static std::string defaultDirectory();
    static slice effectiveDir(slice inDirectory);
    static C4DatabaseConfig2 asC4Config(const CBLDatabaseConfiguration*);

    C4Database* _getC4Database() const;
    Retained<CBLDocument> _getDocument(slice docID, bool isMutable, bool allRevisions) const;
    Retained<CBLListenerToken> addListener(fleece::function_ref<fleece::Retained<CBLListenerToken>()>);
    void databaseChanged();
    void callDBListeners();
    void callDocListeners();

    alloc_slice const dir;
    C4BlobStore* _blobStore;
    std::unique_ptr<C4DatabaseObserver> _observer;
    cbl_internal::Listeners<CBLDatabaseChangeListener> _listeners;
    cbl_internal::Listeners<CBLDatabaseChangeDetailListener> _detailListeners;
    cbl_internal::Listeners<CBLDocumentChangeListener> _docListeners;
    NotificationQueue _notificationQueue;
};
