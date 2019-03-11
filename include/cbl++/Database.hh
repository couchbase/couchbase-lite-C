//
// Database.hh
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
#include "Base.hh"
#include "CBLDatabase.h"
#include "CBLDocument.h"
#include <functional>
#include <vector>

namespace cbl {
    class Document;
    class MutableDocument;


    class Database : private RefCounted {
    public:
        // Static database-file operations:

        static bool exists(const char* _cbl_nonnull name,
                           const char *inDirectory)
        {
            return cbl_databaseExists(name, inDirectory);
        }

        static void copyDatabase(const char* _cbl_nonnull fromPath,
                                 const char* _cbl_nonnull toName)
        {
            CBLError error;
            check( cbl_copyDatabase(fromPath, toName, nullptr, &error), error );
        }

        static void copyDatabase(const char* _cbl_nonnull fromPath,
                                 const char* _cbl_nonnull toName,
                                 const CBLDatabaseConfiguration& config)
        {
            CBLError error;
            check( cbl_copyDatabase(fromPath, toName, &config, &error), error );
        }

        static void deleteDatabase(const char _cbl_nonnull *name,
                                   const char *inDirectory)
        {
            CBLError error;
            check( cbl_deleteDatabase(name, inDirectory, &error), error);
        }

        // Lifecycle:

        Database(const char *name _cbl_nonnull) {
            CBLError error;
            _ref = (CBLRefCounted*) cbl_db_open(name, nullptr, &error);
            check(_ref != nullptr, error);
        }

        Database(const char *name _cbl_nonnull,
                 const CBLDatabaseConfiguration& config)
        {
            CBLError error;
            _ref = (CBLRefCounted*) cbl_db_open(name, &config, &error);
            check(_ref != nullptr, error);
        }

        void close() {
            CBLError error;
            check(cbl_db_close(ref(), &error), error);
        }

        void deleteDatabase() {
            CBLError error;
            check(cbl_db_delete(ref(), &error), error);
        }

        void compact()  {
            CBLError error;
            check(cbl_db_compact(ref(), &error), error);
        }

        // Accessors:

        const char* name() const _cbl_nonnull               {return cbl_db_name(ref());}
        const char* path() const _cbl_nonnull               {return cbl_db_path(ref());}
        uint64_t count() const                              {return cbl_db_count(ref());}
        CBLDatabaseConfiguration config() const             {return cbl_db_config(ref());}

        // Documents:

        inline Document getDocument(const char *id _cbl_nonnull) const;
        inline MutableDocument getMutableDocument(const char *id _cbl_nonnull) const;

        inline Document saveDocument(MutableDocument &doc,
                                     CBLConcurrencyControl c = kCBLConcurrencyControlFailOnConflict);

        time_t getDocumentExpiration(const char *docID) const {
            CBLError error;
            time_t exp = cbl_db_getDocumentExpiration(ref(), docID, &error);
            check(exp >= 0, error);
            return exp;
        }

        void setDocumentExpiration(const char *docID, time_t expiration) {
            CBLError error;
            check(cbl_db_setDocumentExpiration(ref(), docID, expiration, &error), error);
        }

        void purgeDocumentByID(const char *docID) {
            CBLError error;
            check(cbl_db_purgeDocumentByID(ref(), docID, &error), error);
        }

        // Listeners:

        using Listener = cbl::ListenerToken<Database,const std::vector<const char*>&>;

        [[nodiscard]] Listener addListener(Listener::Callback f) {
            auto l = Listener(f);
            l.setToken( cbl_db_addChangeListener(ref(), &_callListener, l.context()) );
            return l;
        }


        using DocumentListener = cbl::ListenerToken<Database,const char*>;

        [[nodiscard]] DocumentListener addDocumentListener(const char *docID,
                                                           DocumentListener::Callback f)
        {
            auto l = DocumentListener(f);
            l.setToken( cbl_db_addDocumentChangeListener(ref(), docID, &_callDocListener, l.context()) );
            return l;
        }

        // Notifications:

        using NotificationsReadyCallback = std::function<void(Database)>;

        void bufferNotifications(NotificationsReadyCallback callback) {
            auto callbackPtr = new NotificationsReadyCallback(callback);    //FIX: This is leaked
            cbl_db_bufferNotifications(ref(),
                                       [](void *context, CBLDatabase *db) {
                                           (*(NotificationsReadyCallback*)context)(Database(db));
                                       },
                                       callbackPtr);
        }

        void sendNotifications()                            {cbl_db_sendNotifications(ref());}

    private:
        static void _callListener(void *context, const CBLDatabase *db,
                                  unsigned nDocs, const char **docIDs)
        {
            std::vector<const char*> vec(&docIDs[0], &docIDs[nDocs]);
            Listener::call(context, Database((CBLDatabase*)db), vec);
        }

        static void _callDocListener(void *context, const CBLDatabase *db, const char *docID) {
            DocumentListener::call(context, Database((CBLDatabase*)db), docID);
        }

        CBL_REFCOUNTED_BOILERPLATE(Database, RefCounted, CBLDatabase)
    };

}
