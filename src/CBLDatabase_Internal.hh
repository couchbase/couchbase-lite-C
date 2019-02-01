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


struct CBLDatabase : public CBLRefCounted {

    CBLDatabase(C4Database* _cbl_nonnull db,
                const std::string &name_,
                const std::string &path_,
                const std::string &dir_)
    :c4db(db)
    ,name(name_)
    ,path(path_)
    ,dir(dir_)
    { }

    virtual ~CBLDatabase();

    C4Database* const c4db;
    std::string const name;
    std::string const path;
    std::string const dir;

    CBLListenerToken* addListener(CBLDatabaseListener listener _cbl_nonnull, void *context);
    CBLListenerToken* addDocListener(const char *docID _cbl_nonnull,
                                     CBLDocumentListener listener _cbl_nonnull, void *context);

    void bufferNotifications(CBLNotificationsReadyCallback callback, void *context);
    void sendNotifications();

    bool shouldNotifyNow();

    C4BlobStore* blobStore() const                      {return c4db_getBlobStore(c4db, nullptr);}

private:
    void databaseChanged();
    void callDBListeners();
    void callDocListeners();

    C4DatabaseObserver* _observer {nullptr};
    cbl_internal::Listeners<CBLDatabaseListener> _listeners;
    cbl_internal::Listeners<CBLDocumentListener> _docListeners;

    CBLNotificationsReadyCallback _notificationsCallback {nullptr};
    void* _notificationsContext;
    bool _notificationsAnnounced {false};
};


namespace cbl_internal {
    static inline C4Database* internal(const CBLDatabase *db)    {return db->c4db;}
}
