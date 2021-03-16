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
    CBLDatabase(C4Database* _cbl_nonnull db,
                slice name_,
                slice dir_)
    :access_lock(std::move(db))
    ,dir(dir_)
    ,_blobStore(&db->getBlobStore())
    ,_notificationQueue(this)
    { }

    virtual ~CBLDatabase() {
        use([&](Retained<C4Database> &c4db) {
            _docListeners.clear();
            _observer = nullptr;
        });
    }

    // Default location for databases. This is platform-dependent.
    static std::string defaultDirectory();

    alloc_slice const dir;

    RetainedConst<CBLDocument> getDocument(slice docID, bool allRevisions, CBLError*) const;

    Retained<CBLDocument> getMutableDocument(slice docID, CBLError*);

    Retained<CBLListenerToken> addListener(CBLDatabaseChangeListener _cbl_nonnull,
                                                   void *ctx);
    Retained<CBLListenerToken> addListener(CBLDatabaseChangeDetailListener _cbl_nonnull,
                                                   void *ctx);

    Retained<CBLListenerToken> addDocListener(slice docID,
                                              CBLDocumentChangeListener _cbl_nonnull,
                                              void *context);

    void notify(Notification n) const   {const_cast<CBLDatabase*>(this)->_notificationQueue.add(n);}
    void sendNotifications()            {_notificationQueue.notifyAll();}

    void bufferNotifications(CBLNotificationsReadyCallback callback, void *context) {
        _notificationQueue.setCallback(callback, context);
    }

    template <class LISTENER, class... Args>
    void notify(ListenerToken<LISTENER> *listener, Args... args) const {
        Retained<ListenerToken<LISTENER>> retained = listener;
        notify([=]() {
            retained->call(args...);
        });
    }

    C4BlobStore* blobStore() const      {return _blobStore;}

private:
    friend struct CBLURLEndpointListener;
    friend class cbl_internal::CBLLocalEndpoint;

    C4Database* _getC4Database() const;
    Retained<CBLDocument> _getDocument(slice docID, bool isMutable, bool allRevisions,
                                       CBLError *outError);
    Retained<CBLListenerToken> addListener(fleece::function_ref<fleece::Retained<CBLListenerToken>()>);
    void databaseChanged();
    void callDBListeners();
    void callDocListeners();

    C4BlobStore* _blobStore;
    std::unique_ptr<C4DatabaseObserver> _observer;
    cbl_internal::Listeners<CBLDatabaseChangeListener> _listeners;
    cbl_internal::Listeners<CBLDatabaseChangeDetailListener> _detailListeners;
    cbl_internal::Listeners<CBLDocumentChangeListener> _docListeners;
    NotificationQueue _notificationQueue;
};
