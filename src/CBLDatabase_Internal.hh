//
//  CBLDatabase_Internal.hh
//  CBL_C
//
//  Created by Jens Alfke on 1/21/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "CBLDatabase.h"
#include "CBLDocument.h"
#include "Internal.hh"
#include "Listener.hh"
#include "access_lock.hh"


struct CBLDatabase : public CBLRefCounted {

    CBLDatabase(C4Database* _cbl_nonnull db,
                const std::string &name_,
                const std::string &dir_)
    :c4db(db)
    ,name(name_)
    ,path(fleece::alloc_slice(c4db_getPath(c4db)))
    ,dir(dir_)
    ,_notificationQueue(this)
    { }

    virtual ~CBLDatabase() {
        c4dbobs_free(_observer);
        _docListeners.clear();
        c4db_release(c4db);
    }

    C4Database* const c4db;
    std::string const name;         // Cached copy so API can return a C string
    std::string const path;         // Cached copy so API can return a C string
    std::string const dir;          // Cached copy so API can return a C string

    CBLListenerToken* addListener(CBLDatabaseChangeListener listener _cbl_nonnull, void *context);
    CBLListenerToken* addDocListener(const char *docID _cbl_nonnull,
                                     CBLDocumentChangeListener listener _cbl_nonnull, void *context);

    void notify(Notification n) const   {const_cast<CBLDatabase*>(this)->_notificationQueue.add(n);}
    void sendNotifications()            {_notificationQueue.notifyAll();}

    void bufferNotifications(CBLNotificationsReadyCallback callback, void *context) {
        _notificationQueue.setCallback(callback, context);
    }

    template <class LISTENER, class... Args>
    void notify(ListenerToken<LISTENER> *listener, Args... args) const {
        fleece::Retained<ListenerToken<LISTENER>> retained = listener;
        notify([=]() {
            retained->call(args...);
        });
    }

    C4BlobStore* blobStore() const                      {return c4db_getBlobStore(c4db, nullptr);}

private:
    void databaseChanged();
    void callDBListeners();
    void callDocListeners();

    C4DatabaseObserver* _observer {nullptr};
    cbl_internal::Listeners<CBLDatabaseChangeListener> _listeners;
    cbl_internal::Listeners<CBLDocumentChangeListener> _docListeners;
    NotificationQueue _notificationQueue;
};


namespace cbl_internal {
    static inline C4Database* internal(const CBLDatabase *db)    {return db->c4db;}
}
