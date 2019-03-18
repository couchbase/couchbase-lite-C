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
            return CBL_DatabaseExists(name, inDirectory);
        }

        static void copyDatabase(const char* _cbl_nonnull fromPath,
                                 const char* _cbl_nonnull toName)
        {
            CBLError error;
            check( CBL_CopyDatabase(fromPath, toName, nullptr, &error), error );
        }

        static void copyDatabase(const char* _cbl_nonnull fromPath,
                                 const char* _cbl_nonnull toName,
                                 const CBLDatabaseConfiguration& config)
        {
            CBLError error;
            check( CBL_CopyDatabase(fromPath, toName, &config, &error), error );
        }

        static void deleteDatabase(const char _cbl_nonnull *name,
                                   const char *inDirectory)
        {
            CBLError error;
            check( CBL_DeleteDatabase(name, inDirectory, &error), error);
        }

        // Lifecycle:

        Database(const char *name _cbl_nonnull) {
            CBLError error;
            _ref = (CBLRefCounted*) CBLDatabase_Open(name, nullptr, &error);
            check(_ref != nullptr, error);
        }

        Database(const char *name _cbl_nonnull,
                 const CBLDatabaseConfiguration& config)
        {
            CBLError error;
            _ref = (CBLRefCounted*) CBLDatabase_Open(name, &config, &error);
            check(_ref != nullptr, error);
        }

        void close() {
            CBLError error;
            check(CBLDatabase_Close(ref(), &error), error);
        }

        void deleteDatabase() {
            CBLError error;
            check(CBLDatabase_Delete(ref(), &error), error);
        }

        void compact()  {
            CBLError error;
            check(CBLDatabase_Compact(ref(), &error), error);
        }

        // Accessors:

        const char* name() const _cbl_nonnull               {return CBLDatabase_Name(ref());}
        const char* path() const _cbl_nonnull               {return CBLDatabase_Path(ref());}
        uint64_t count() const                              {return CBLDatabase_Count(ref());}
        CBLDatabaseConfiguration config() const             {return CBLDatabase_Config(ref());}

        // Documents:

        inline Document getDocument(const char *id _cbl_nonnull) const;
        inline MutableDocument getMutableDocument(const char *id _cbl_nonnull) const;

        inline Document saveDocument(MutableDocument &doc,
                                     CBLConcurrencyControl c = kCBLConcurrencyControlFailOnConflict);

        time_t getDocumentExpiration(const char *docID) const {
            CBLError error;
            time_t exp = CBLDatabase_GetDocumentExpiration(ref(), docID, &error);
            check(exp >= 0, error);
            return exp;
        }

        void setDocumentExpiration(const char *docID, time_t expiration) {
            CBLError error;
            check(CBLDatabase_SetDocumentExpiration(ref(), docID, expiration, &error), error);
        }

        void purgeDocumentByID(const char *docID) {
            CBLError error;
            check(CBLDatabase_PurgeDocumentByID(ref(), docID, &error), error);
        }

        // Listeners:

        using Listener = cbl::ListenerToken<Database,const std::vector<const char*>&>;

        [[nodiscard]] Listener addListener(Listener::Callback f) {
            auto l = Listener(f);
            l.setToken( CBLDatabase_AddChangeListener(ref(), &_callListener, l.context()) );
            return l;
        }


        using DocumentListener = cbl::ListenerToken<Database,const char*>;

        [[nodiscard]] DocumentListener addDocumentListener(const char *docID,
                                                           DocumentListener::Callback f)
        {
            auto l = DocumentListener(f);
            l.setToken( CBLDatabase_AddDocumentChangeListener(ref(), docID, &_callDocListener, l.context()) );
            return l;
        }

        // Notifications:

        using NotificationsReadyCallback = std::function<void(Database)>;

        void bufferNotifications(NotificationsReadyCallback callback) {
            auto callbackPtr = new NotificationsReadyCallback(callback);    //FIX: This is leaked
            CBLDatabase_BufferNotifications(ref(),
                                       [](void *context, CBLDatabase *db) {
                                           (*(NotificationsReadyCallback*)context)(Database(db));
                                       },
                                       callbackPtr);
        }

        void sendNotifications()                            {CBLDatabase_SendNotifications(ref());}

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
