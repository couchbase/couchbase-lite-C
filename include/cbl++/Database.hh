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
#include "cbl++/Base.hh"
#include "cbl/CBLDatabase.h"
#include "cbl/CBLDocument.h"
#include "cbl/CBLQuery.h"
#include "cbl/CBLLog.h"
#include "fleece/Mutable.hh"
#include <functional>
#include <string>
#include <vector>

// PLEASE NOTE: This C++ wrapper API is provided as a convenience only.
// It is not considered part of the official Couchbase Lite API.

CBL_ASSUME_NONNULL_BEGIN

namespace cbl {
    class Document;
    class MutableDocument;


    using SaveConflictHandler = std::function<bool(MutableDocument documentBeingSaved,
                                                   Document conflictingDocument)>;


    class Database : private RefCounted {
    public:
        // Static database-file operations:

        static bool exists(slice name,
                           slice inDirectory)
        {
            return CBL_DatabaseExists(name, inDirectory);
        }

        static void copyDatabase(slice fromPath,
                                 slice toName)
        {
            CBLError error;
            check( CBL_CopyDatabase(fromPath, toName,
                                    nullptr, &error), error );
        }

        static void copyDatabase(slice fromPath,
                                 slice toName,
                                 const CBLDatabaseConfiguration& config)
        {
            CBLError error;
            check( CBL_CopyDatabase(fromPath, toName,
                                    &config, &error), error );
        }

        static void deleteDatabase(slice name,
                                   slice inDirectory)
        {
            CBLError error;
            if (!CBL_DeleteDatabase(name,
                                    inDirectory,
                                    &error) && error.code != 0)
                check(false, error);
        }

        // Lifecycle:

        Database(slice name) {
            CBLError error;
            _ref = (CBLRefCounted*) CBLDatabase_Open(name, nullptr, &error);
            check(_ref != nullptr, error);
        }

        Database(slice name,
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
        
        void performMaintenance(CBLMaintenanceType type) {
            CBLError error;
            check(CBLDatabase_PerformMaintenance(ref(), type, &error), error);
        }

        // Accessors:

        std::string name() const                        {return asString(CBLDatabase_Name(ref()));}
        std::string path() const                        {return asString(CBLDatabase_Path(ref()));}
        uint64_t count() const                          {return CBLDatabase_Count(ref());}
        CBLDatabaseConfiguration config() const         {return CBLDatabase_Config(ref());}

        // Documents:

        inline Document getDocument(slice id) const;
        inline MutableDocument getMutableDocument(slice id) const;

        inline void saveDocument(MutableDocument &doc);

        _cbl_warn_unused
        inline bool saveDocument(MutableDocument &doc, CBLConcurrencyControl c);

        _cbl_warn_unused
        inline bool saveDocument(MutableDocument &doc, SaveConflictHandler conflictHandler);

        inline void deleteDocument(Document &doc);

        _cbl_warn_unused
        inline bool deleteDocument(Document &doc, CBLConcurrencyControl c);

        time_t getDocumentExpiration(slice docID) const {
            CBLError error;
            time_t exp = CBLDatabase_GetDocumentExpiration(ref(), docID, &error);
            check(exp >= 0, error);
            return exp;
        }

        void setDocumentExpiration(slice docID, time_t expiration) {
            CBLError error;
            check(CBLDatabase_SetDocumentExpiration(ref(), docID, expiration, &error), error);
        }

        inline void purgeDocument(Document &doc);

        bool purgeDocumentByID(slice docID) {
            CBLError error;
            bool purged = CBLDatabase_PurgeDocumentByID(ref(), docID, &error);
            if (!purged && error.code != 0)
                throw error;
            return purged;
        }

        // Indexes:

        void createValueIndex(slice name, CBLValueIndexConfiguration config) {
            CBLError error;
            check(CBLDatabase_CreateValueIndex(ref(), name, config, &error), error);
        }
        
        void createFullTextIndex(slice name, CBLFullTextIndexConfiguration config) {
            CBLError error;
            check(CBLDatabase_CreateFullTextIndex(ref(), name, config, &error), error);
        }

        void deleteIndex(slice name) {
            CBLError error;
            check(CBLDatabase_DeleteIndex(ref(), name, &error), error);
        }

        fleece::Array getIndexNames() {
            FLArray flNames = CBLDatabase_GetIndexNames(ref());
            fleece::Array names(flNames);
            FLArray_Release(flNames);
            return names;
        }

        // Listeners:

        using Listener = cbl::ListenerToken<Database,const std::vector<slice>&>;

        [[nodiscard]] Listener addListener(Listener::Callback f) {
            auto l = Listener(f);
            l.setToken( CBLDatabase_AddChangeListener(ref(), &_callListener, l.context()) );
            return l;
        }


        using DocumentListener = cbl::ListenerToken<Database,slice>;

        [[nodiscard]] DocumentListener addDocumentListener(slice docID,
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
        static void _callListener(void* _cbl_nullable context,
                                  const CBLDatabase *db,
                                  unsigned nDocs, FLString *docIDs)
        {
            std::vector<slice> vec((slice*)&docIDs[0], (slice*)&docIDs[nDocs]);
            Listener::call(context, Database((CBLDatabase*)db), vec);
        }

        static void _callDocListener(void* _cbl_nullable context,
                                     const CBLDatabase *db, FLString docID) {
            DocumentListener::call(context, Database((CBLDatabase*)db), docID);
        }

        CBL_REFCOUNTED_BOILERPLATE(Database, RefCounted, CBLDatabase)
    };


    /** A helper object for database transactions.
        A Transaction object should be declared as a local (auto) variable.
        You must explicitly call \ref commit to commit changes; if you don't, the transaction
        will abort when it goes out of scope. */
    class Transaction {
    public:
        /** Begins a batch operation on the database that will end when the Batch instance
            goes out of scope. */
        explicit Transaction(Database db)
        :Transaction(db.ref())
        { }

        explicit Transaction (CBLDatabase *db) {
            CBLError error;
            RefCounted::check(CBLDatabase_BeginTransaction(db, &error), error);
            _db = db;
        }

        /** Commits changes and ends the transaction. */
        void commit()   {end(true);}

        /** Ends the transaction, rolling back changes. */
        void abort()    {end(false);}

        ~Transaction()  {end(false);}

    private:
        void end(bool commit) {
            CBLDatabase *db = _db;
            if (db) {
                _db = nullptr;
                CBLError error;
                if (!CBLDatabase_EndTransaction(db, commit, &error)) {
                    // If an exception is thrown while a Batch is in scope, its destructor will
                    // call end(). If I'm in this situation I cannot throw another exception or
                    // the C++ runtime will abort the process. Detect this and just warn instead.
                    if (std::current_exception())
                        CBL_Log(kCBLLogDomainDatabase, kCBLLogWarning,
                                "Transaction::end failed, while handling an exception");
                    else
                        RefCounted::check(false, error);
                }
            }
        }

        CBLDatabase* _cbl_nullable _db = nullptr;
    };

}

CBL_ASSUME_NONNULL_END
