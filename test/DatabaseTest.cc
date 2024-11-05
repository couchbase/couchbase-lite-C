//
// DatabaseTest.cc
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

#include "CBLTest.hh"
#include "CBLPrivate.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <mutex>
#include <string>
#include <thread>

using namespace std;
using namespace fleece;

static constexpr const slice kOtherDBName = "CBLTest_OtherDB";

static int dbListenerCalls = 0;
static int fooListenerCalls = 0;
static int barListenerCalls = 0;
static int notificationsReadyCalls = 0;
static mutex _sListenerMutex;

static void notificationsReady(void *context, CBLDatabase* db) {
    lock_guard<mutex> lock(_sListenerMutex);
    
    ++notificationsReadyCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
}

static void dbListener(void *context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs) {
    lock_guard<mutex> lock(_sListenerMutex);
    
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 1);
    CHECK(slice(docIDs[0]) == "foo"_sl);
}

static void dbListenerForBufferNotification(void *context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs) {
    lock_guard<mutex> lock(_sListenerMutex);
    
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 2);
    CHECK(docIDs[0] == "foo"_sl);
    CHECK(docIDs[1] == "bar"_sl);
}

static void dbListenerWithDelay(void *context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs) {
     lock_guard<mutex> lock(_sListenerMutex);

     this_thread::sleep_for(1000ms);

     ++dbListenerCalls;
     auto test = (CBLTest*)context;
     CHECK(test->db == db);
}

static void fooListener(void *context, const CBLDatabase *db, FLString docID) {
    lock_guard<mutex> lock(_sListenerMutex);
    
    ++fooListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(slice(docID) == "foo"_sl);
}

static void barListener(void *context, const CBLDatabase *db, FLString docID) {
    lock_guard<mutex> lock(_sListenerMutex);
    
    ++barListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(docID == "bar"_sl);
}

class DatabaseTest : public CBLTest {
public:
    CBLDatabase* otherDB = nullptr;
    CBLCollection* otherDBDefaultCol = nullptr;

    DatabaseTest() {
        CHECK(CBLCollection_Count(defaultCollection) == 0);

        CBLError error;
        auto config = databaseConfig();
        if (!CBL_DeleteDatabase(kOtherDBName, config.directory, &error) && error.code != 0)
            FAIL("Can't delete otherDB database: " << error.domain << "/" << error.code);

        otherDB = CBLDatabase_Open(kOtherDBName, &config, &error);

        otherDBDefaultCol = CBLDatabase_DefaultCollection(otherDB, &error);
        if (!otherDBDefaultCol) {
            FAIL("_default collection not found for otherDB: " << error.domain << "/" << error.code);
        }
        CHECK(CBLCollection_Count(otherDBDefaultCol) == 0);
    }

    ~DatabaseTest() {
        CBLCollection_Release(otherDBDefaultCol);
        if (otherDB) {
            CBLError error;
            if (!CBLDatabase_Close(otherDB, &error))
                WARN("Failed to close other database: " << error.domain << "/" << error.code);
            CBLDatabase_Release(otherDB);
        }
    }
    
    void testInvalidDatabase() {
        CBLError error;
        REQUIRE(db);
        
        ExpectingExceptions x;
        
        // Properties:
        CHECK(CBLDatabase_Name(db));
        CHECK(CBLDatabase_Path(db) == kFLSliceNull);
        CHECK(CBLDatabase_LastSequence(db) == 0);
        CHECK(CBLCollection_Count(defaultCollection) == 0);
        
        // Life Cycle:
        error = {};
        CHECK(CBLDatabase_Close(db, &error));
        CHECK(error.code == 0);
        
        error = {};
        CHECK(!CBLDatabase_Delete(db, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_BeginTransaction(db, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_EndTransaction(db, false, &error));
        CheckNotOpenError(error);
        
    #ifdef COUCHBASE_ENTERPRISE
        error = {};
        CHECK(!CBLDatabase_ChangeEncryptionKey(db, NULL, &error));
        CheckNotOpenError(error);
    #endif
        
        error = {};
        CHECK(!CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeIntegrityCheck, &error));
        CheckNotOpenError(error);
        
        // Document Functions:
        error = {};
        auto doc = CBLDocument_CreateWithID("doc1"_sl);
        CHECK(!CBLCollection_SaveDocument(defaultCollection, doc, &error));
        CheckNotOpenError(error);
        
        error = {};
        auto conflictHandler = [](void *c, CBLDocument* d1, const CBLDocument* d2) -> bool { return true; };
        CHECK(!CBLCollection_SaveDocumentWithConflictHandler(defaultCollection, doc, conflictHandler, nullptr, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_SaveDocumentWithConcurrencyControl(defaultCollection, doc, kCBLConcurrencyControlLastWriteWins, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetDocument(defaultCollection, "doc1"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetMutableDocument(defaultCollection, "doc1"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_DeleteDocument(defaultCollection, doc, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_DeleteDocumentWithConcurrencyControl(defaultCollection, doc, kCBLConcurrencyControlLastWriteWins, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_PurgeDocument(defaultCollection, doc, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_PurgeDocumentByID(defaultCollection, "doc1"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(CBLCollection_GetDocumentExpiration(defaultCollection, "doc1"_sl, &error) == -1);
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_SetDocumentExpiration(defaultCollection, "doc1"_sl, CBL_Now(), &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_CreateValueIndex(defaultCollection, "Value"_sl, {}, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_CreateFullTextIndex(defaultCollection, "FTS"_sl, {}, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetIndexNames(defaultCollection, &error));
        CheckNotOpenError(error);
        
        auto token = CBLDatabase_AddChangeListener(db, dbListener, this);
        CHECK(token);
        CBLListener_Remove(token);
        
        auto docToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);
        CHECK(docToken);
        CBLListener_Remove(docToken);
        
        CBLDocument_Release(doc);
    }
};

    TEST_CASE_METHOD(DatabaseTest, "Database") {
    auto dbDir = databaseDir();
    CHECK(CBLDatabase_Name(db) == kDatabaseName);
    CHECK(string(CBLDatabase_Path(db)) == string(dbDir) + kPathSeparator + string(kDatabaseName) + ".cblite2" + kPathSeparator);
    CHECK(CBL_DatabaseExists(kDatabaseName, dbDir));
    CHECK(CBLCollection_Count(defaultCollection) == 0);
    CHECK(CBLDatabase_LastSequence(db) == 0);       // not public API
}

TEST_CASE_METHOD(DatabaseTest, "Database w/o config") {
    CBLError error;
    CBLDatabase *defaultdb = CBLDatabase_Open("unconfig"_sl, nullptr, &error);
    REQUIRE(defaultdb);
    alloc_slice path = CBLDatabase_Path(defaultdb);
    cerr << "Default database is at " << path << "\n";
    CHECK(CBL_DatabaseExists("unconfig"_sl, nullslice));

    CBLDatabaseConfiguration config = CBLDatabase_Config(defaultdb);
    CHECK(config.directory != nullslice);     // exact value is platform-specific
#ifdef COUCHBASE_ENTERPRISE
    CHECK(config.encryptionKey.algorithm == kCBLEncryptionNone);
#endif
    CHECK(CBLDatabase_Delete(defaultdb, &error));
    CBLDatabase_Release(defaultdb);

    CHECK(!CBL_DatabaseExists("unconfig"_sl, nullslice));
}


#ifdef COUCHBASE_ENTERPRISE

TEST_CASE_METHOD(DatabaseTest, "Database Encryption") {
    // Ensure no database:
    CBL_DeleteDatabase("encdb"_sl, nullslice, nullptr);
    CHECK(!CBL_DatabaseExists("encdb"_sl, nullslice));
    
    // Correct key:
    CBLError error;
    CBLEncryptionKey key;
    
    bool useSHA256Key = true;
    SECTION("SHA-265 Key")
    {
        CBLEncryptionKey_FromPassword(&key, "sekrit"_sl);
    }
    
    SECTION("SHA-1 Key")
    {
        useSHA256Key = false;
        CBLEncryptionKey_FromPasswordOld(&key, "sekrit"_sl);
    }
    
    CBLDatabaseConfiguration config = {nullslice, key};
    CBLDatabase *defaultdb = CBLDatabase_Open("encdb"_sl, &config, &error);
    REQUIRE(defaultdb);
    alloc_slice path = CBLDatabase_Path(defaultdb);
    cerr << "Default database is at " << path << "\n";
    CHECK(CBL_DatabaseExists("encdb"_sl, nullslice));

    CBLDatabaseConfiguration config1 = CBLDatabase_Config(defaultdb);
    REQUIRE(config1.encryptionKey.algorithm  == key.algorithm);
    REQUIRE(memcmp(config1.encryptionKey.bytes, key.bytes, 32) == 0);
    
    // Correct key from config:
    CBLDatabase *correctkeydb = CBLDatabase_Open("encdb"_sl, &config1, &error);
    REQUIRE(correctkeydb);
    CBLDatabase_Release(correctkeydb);
    
    // No key:
    {
        ExpectingExceptions x;
        CBLDatabase *nokeydb = CBLDatabase_Open("encdb"_sl, nullptr, &error);
        REQUIRE(nokeydb == nullptr);
        CHECK(error.domain == kCBLDomain);
        CHECK(error.code == kCBLErrorNotADatabaseFile);
    }
    
    // Wrong key:
    {
        ExpectingExceptions x;
        CBLEncryptionKey key2;
        
        if (useSHA256Key) {
            CBLEncryptionKey_FromPassword(&key2, "wrongpassword"_sl);
        } else {
            CBLEncryptionKey_FromPasswordOld(&key2, "wrongpassword"_sl);
        }
        
        CBLDatabaseConfiguration config2 = {nullslice, key2};
        CBLDatabase *wrongkeydb = CBLDatabase_Open("encdb"_sl, &config2, &error);
        REQUIRE(wrongkeydb == nullptr);
        CHECK(error.domain == kCBLDomain);
        CHECK(error.code == kCBLErrorNotADatabaseFile);
    }

    CHECK(CBLDatabase_Delete(defaultdb, &error));
    CBLDatabase_Release(defaultdb);
    CHECK(!CBL_DatabaseExists("encdb"_sl, nullslice));
}

#endif

#pragma mark - Full Sync:

/** Test Spec for Database Full Sync Option
    https://github.com/couchbaselabs/couchbase-lite-api/blob/master/spec/tests/T0003-SQLite-Options.md
    v. 2.0.0 */

/**
 1. TestSQLiteFullSyncConfig

 Description

 Test that the FullSync default is as expected and that it's setter and getter work.

 Steps

 1. Create a DatabaseConfiguration object.
 2. Get and check the value of the FullSync property: it should be false.
 3. Set the FullSync property true
 4. Get the config FullSync property and verify that it is true
 5. Set the FullSync property false
 6. Get the config FullSync property and verify that it is false */
TEST_CASE_METHOD(DatabaseTest, "TestSQLiteFullSyncConfig") {
    auto config = CBLDatabaseConfiguration_Default();
    CHECK(!config.fullSync);
    
    config.fullSync = true;
    CHECK(config.fullSync);
    
    config.fullSync = false;
    CHECK(!config.fullSync);
}

/**
 2. TestDBWithFullSync

 Description

 Test that a Database respects the FullSync property

 Steps

 1. Create a DatabaseConfiguration object and set Full Sync false
 2. Create a database with the config
 3. Get the configuration object from the Database and verify that FullSync is false
 4. Use c4db_config2 (perhaps necessary only for this test) to confirm that its config does not contain the kC4DB_DiskSyncFull flag
 5. Set the config's FullSync property true
 6. Create a database with the config
 7. Get the configuration object from the Database and verify that FullSync is true
 8. Use c4db_config2 to confirm that its config contains the kC4DB_DiskSyncFull flag
 */
TEST_CASE_METHOD(DatabaseTest, "TestDBWithFullSync") {
    auto config = databaseConfig();
    
    auto dbname = "fullsyncdb"_sl;
    CBL_DeleteDatabase(dbname, config.directory, nullptr);
    CHECK(!CBL_DatabaseExists(dbname, config.directory));
    
    CBLError error{};
    config.fullSync = false;
    CBLDatabase* db = CBLDatabase_Open(dbname, &config, &error);
    REQUIRE(db);
    CHECK(!CBLDatabase_Config(db).fullSync);
    CHECK(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    
    config.fullSync = true;
    db = CBLDatabase_Open(dbname, &config, &error);
    REQUIRE(db);
    CHECK(CBLDatabase_Config(db).fullSync);
    CHECK(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
}

#pragma mark - Save Document:

TEST_CASE_METHOD(DatabaseTest, "Save Document into Different DB Instance") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    
    CBLError error;
    CHECK(CBLCollection_SaveDocument(defaultCollection, doc, &error));
    
    ExpectingExceptions x;
    CHECK(!CBLCollection_SaveDocument(otherDBDefaultCol, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
}

#pragma mark - Delete Document:

TEST_CASE_METHOD(DatabaseTest, "Delete Document from Different DB Instance") {
    createDocWithPair(db, "doc1", "foo", "bar");
    CBLError error;
    const CBLDocument* doc = CBLCollection_GetDocument(defaultCollection, "doc1"_sl, &error);
    REQUIRE(doc);

    ExpectingExceptions x;
    CHECK(!CBLCollection_DeleteDocument(otherDBDefaultCol, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
}

#pragma mark - Purge Document:

TEST_CASE_METHOD(DatabaseTest, "Purge Document from Different DB Instance") {
    createDocWithPair(db, "doc1", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLCollection_GetDocument(defaultCollection, "doc1"_sl, &error);
    REQUIRE(doc);
    
    ExpectingExceptions x;
    CHECK(!CBLCollection_PurgeDocument(otherDBDefaultCol, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
}

#pragma mark - File Operations:


TEST_CASE_METHOD(DatabaseTest, "Copy Database") {
    createDocWithPair(db, "foo", "greeting", "Howdy!");

    CBLError error;
    auto config = databaseConfig();
    auto dir = databaseDir();

    CBL_DeleteDatabase("copy"_sl, dir, &error);
    REQUIRE(!CBL_DatabaseExists("copy"_sl, config.directory));

    // Copy:
    alloc_slice path = CBLDatabase_Path(db);
    REQUIRE(CBL_CopyDatabase(path, "copy"_sl, &config, &error));
    
    // Check:
    CHECK(CBL_DatabaseExists("copy"_sl, config.directory));
    CBLDatabase* copyDB = CBLDatabase_Open("copy"_sl, &config, &error);
    CHECK(copyDB != nullptr);
    CBLCollection* copyCol = CBLDatabase_DefaultCollection(copyDB, &error);
    CHECK(CBLCollection_Count(copyCol) == 1);

    CBLDocument* doc = CBLCollection_GetMutableDocument(copyCol, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CBLDocument_Release(doc);

    CBLCollection_Release(copyCol);
    if (!CBLDatabase_Close(copyDB, &error))
                WARN("Failed to close other database: " << error.domain << "/" << error.code);
    CBLDatabase_Release(copyDB);
}

#pragma mark - Maintenance:

TEST_CASE_METHOD(DatabaseTest, "Maintenance : Compact and Integrity Check") {
    // Create a doc with blob:
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict dict = CBLDocument_MutableProperties(doc);
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob *blob1 = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    FLMutableDict_SetBlob(dict, FLStr("blob"), blob1);
    
    // Save doc:
    CBLError error;
    REQUIRE(CBLCollection_SaveDocumentWithConcurrencyControl(defaultCollection, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLBlob_Release(blob1);
    CBLDocument_Release(doc);
    
    // Compact:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    
    // Make sure the blob still exists after compact: (issue #73)
    doc = CBLCollection_GetMutableDocument(defaultCollection, "doc1"_sl, &error);
    REQUIRE(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(CBLDocument_Properties(doc), FLStr("blob")));
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    FLSliceResult_Release(content);
    
    // https://issues.couchbase.com/browse/CBL-1617
    // CBLBlob_Release(blob2);
    
    // Delete doc:
    CHECK(CBLCollection_DeleteDocumentWithConcurrencyControl(defaultCollection, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLDocument_Release(doc);
    
    // Compact:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    
    // Integrity check:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeIntegrityCheck, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Maintenance : Reindex") {
    CBLError error;
    CBLValueIndexConfiguration config = {};
    config.expressionLanguage = kCBLJSONLanguage;
    config.expressions = R"([".foo"])"_sl;
    CHECK(CBLCollection_CreateValueIndex(defaultCollection, "foo"_sl, config, &error));
    
    createDocWithPair(db, "doc1", "foo", "bar1");
    createDocWithPair(db, "doc2", "foo", "bar2");
    createDocWithPair(db, "doc3", "foo", "bar3");
    
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeReindex, &error));
    
    FLArray names = CBLCollection_GetIndexNames(defaultCollection, &error);
    REQUIRE(names);
    CHECK(FLArray_Count(names) == 1);
    CHECK(FLValue_AsString(FLArray_Get(names, 0)) == "foo"_sl);
    FLArray_Release(names);
}


TEST_CASE_METHOD(DatabaseTest, "Maintenance : Optimize") {
    CBLValueIndexConfiguration index1 = {};
    index1.expressionLanguage = kCBLN1QLLanguage;
    index1.expressions = "name.first"_sl;
    CBLError error;
    CHECK(CBLCollection_CreateValueIndex(defaultCollection, "index1"_sl, index1, &error));
    
    ImportJSONLines("names_100.json", defaultCollection);
    
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeOptimize, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Maintenance : FullOptimize") {
    CBLValueIndexConfiguration index1 = {};
    index1.expressionLanguage = kCBLN1QLLanguage;
    index1.expressions = "name.first"_sl;
    CBLError error;
    CHECK(CBLCollection_CreateValueIndex(defaultCollection, "index1"_sl, index1, &error));

    ImportJSONLines("names_100.json", defaultCollection);

    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeFullOptimize, &error));
}


#pragma mark - Transaction:


TEST_CASE_METHOD(DatabaseTest, "Transaction Commit") {
    createDocWithPair(db, "doc1", "foo", "bar1");
    createDocWithPair(db, "doc2", "foo", "bar2");

    CHECK(CBLCollection_Count(defaultCollection) == 2);

    // Begin Transaction:
    CBLError error;
    REQUIRE(CBLDatabase_BeginTransaction(db, &error));

    // Create:
    createDocWithPair(db, "doc3", "foo", "bar3");

    // Delete:
    const CBLDocument* doc1 = CBLCollection_GetDocument(defaultCollection, "doc1"_sl, &error);
    REQUIRE(doc1);
    REQUIRE(CBLCollection_DeleteDocument(defaultCollection, doc1, &error));
    CBLDocument_Release(doc1);

    // Purge:
    const CBLDocument* doc2 = CBLCollection_GetDocument(defaultCollection, "doc2"_sl, &error);
    REQUIRE(doc2);
    REQUIRE(CBLCollection_PurgeDocument(defaultCollection, doc2, &error));
    CBLDocument_Release(doc2);

    // Commit Transaction:
    REQUIRE(CBLDatabase_EndTransaction(db, true, &error));

    // Check:
    CHECK(CBLCollection_Count(defaultCollection) == 1);
    const CBLDocument* doc3 = CBLCollection_GetDocument(defaultCollection, "doc3"_sl, &error);
    CHECK(CBLDocument_ID(doc3) == "doc3"_sl);
    CHECK(CBLDocument_Sequence(doc3) == 3);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc3)) == "{\"foo\":\"bar3\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc3)).toJSONString() == "{\"foo\":\"bar3\"}");
    CBLDocument_Release(doc3);

    CHECK(!CBLCollection_GetDocument(defaultCollection, "doc1"_sl, &error));
    CHECK(!CBLCollection_GetDocument(defaultCollection, "doc2"_sl, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Transaction Abort") {
    createDocWithPair(db, "doc1", "foo", "bar1");
    createDocWithPair(db, "doc2", "foo", "bar2");
    
    // Begin Transaction:
    CBLError error;
    REQUIRE(CBLDatabase_BeginTransaction(db, &error));

    // Create:
    createDocWithPair(db, "doc3", "foo", "bar3");

    // Delete:
    const CBLDocument* doc1 = CBLCollection_GetDocument(defaultCollection, "doc1"_sl, &error);
    REQUIRE(doc1);
    REQUIRE(CBLCollection_DeleteDocument(defaultCollection, doc1, &error));
    CBLDocument_Release(doc1);

    // Purge:
    const CBLDocument* doc2 = CBLCollection_GetDocument(defaultCollection, "doc2"_sl, &error);
    REQUIRE(doc2);
    REQUIRE(CBLCollection_PurgeDocument(defaultCollection, doc2, &error));
    CBLDocument_Release(doc2);

    // Abort Transaction:
    REQUIRE(CBLDatabase_EndTransaction(db, false, &error));
    
    CHECK(CBLCollection_Count(defaultCollection) == 2);
    doc1 = CBLCollection_GetMutableDocument(defaultCollection, "doc1"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "doc1"_sl);
    CBLDocument_Release(doc1);
    
    doc2 = CBLCollection_GetMutableDocument(defaultCollection, "doc2"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "doc2"_sl);
    CBLDocument_Release(doc2);
}


#pragma mark - LISTENERS:

TEST_CASE_METHOD(DatabaseTest, "Database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener, this);
    auto docToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);

    // Create a doc, check that the listener was called:
    createDocWithPair(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    CBLListener_Remove(token);
    CBLListener_Remove(docToken);

    // After being removed, the listener should not be called:
    dbListenerCalls = fooListenerCalls = 0;
    createDocWithPair(db, "bar", "greeting", "yo.");
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
}

TEST_CASE_METHOD(DatabaseTest, "Remove Database Listener after releasing database") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener, this);
    auto docToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);

    // Create a doc, check that the listener was called:
    createDocWithPair(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    // Close and release the database:
    CBLError error;
    if (!CBLDatabase_Close(db, &error))
        WARN("Failed to close database: " << error.domain << "/" << error.code);
    CBLDatabase_Release(db);
    db = nullptr;

    // Remove and release the token:
    ExpectingExceptions x;
    CBLListener_Remove(token);
    CBLListener_Remove(docToken);
}

TEST_CASE_METHOD(DatabaseTest, "Scheduled database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = barListenerCalls = 0;

    auto token = CBLDatabase_AddChangeListener(db, dbListenerForBufferNotification, this);
    auto fooToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);
    auto barToken = CBLDatabase_AddDocumentChangeListener(db, "bar"_sl, barListener, this);

    CBLDatabase_BufferNotifications(db, notificationsReady, this);

    // Create two docs; no listeners should be called yet:
    createDocWithPair(db, "foo", "greeting", "Howdy!");
    CHECK(notificationsReadyCalls == 1);
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    createDocWithPair(db, "bar", "greeting", "yo.");
    CHECK(notificationsReadyCalls == 1);
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    // Now the listeners will be called:
    CBLDatabase_SendNotifications(db);
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    // There should be no more notifications:
    CBLDatabase_SendNotifications(db);
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    CBLListener_Remove(token);
    CBLListener_Remove(fooToken);
    CBLListener_Remove(barToken);
}

// CBSE-16738
TEST_CASE_METHOD(DatabaseTest, "Legacy - Database change notifications from different db threads") {
    CBLError error {};
    auto config = databaseConfig();
    auto anotherDB = CBLDatabase_Open(kDatabaseName, &config, &error);
    REQUIRE(anotherDB);
    
    // Add a listener:
    dbListenerCalls = fooListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListenerWithDelay, this);
    
    auto createDoc = [&] (CBLDatabase* database)
    {
        CBLError error {};
        CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
        MutableDict props = CBLDocument_MutableProperties(doc);
        props["greeting"] = "hello";
        CBLDatabase_SaveDocument(database, doc, &error);
        CBLDocument_Release(doc);
    };
    
    thread t1([&]() { createDoc(db); });
    thread t2([=]() { createDoc(anotherDB); });
    
    t1.join();
    t2.join();
    
    CHECK(dbListenerCalls == 2);
    CBLListener_Remove(token);
    
    CBLDatabase_Close(anotherDB, &error);
    CBLDatabase_Release(anotherDB);
}

#pragma mark - BLOBS:

TEST_CASE_METHOD(DatabaseTest, "Save blob read from database", "[Blob]") {
    // Create blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    CHECK(CBLDatabase_SaveBlob(db, blob, &error));
    
    // Save doc with blob:
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict docProps = CBLDocument_MutableProperties(doc);
    FLSlot_SetDict(FLMutableDict_Set(docProps, FLStr("blob")), CBLBlob_Properties(blob));
    CHECK(CBLCollection_SaveDocument(defaultCollection, doc, &error));
    CBLDocument_Release(doc);
    CBLBlob_Release(blob);
    
    // Get blob from the saved doc:
    doc = CBLCollection_GetMutableDocument(defaultCollection, "doc1"_sl, &error);
    CHECK(doc);
    docProps = CBLDocument_MutableProperties(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(docProps, "blob"_sl));
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    
    // Try to save blob from the saved doc:
    ExpectingExceptions x;
    CHECK(!CBLDatabase_SaveBlob(db, const_cast<CBLBlob*>(blob2), &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorUnsupported);
    FLSliceResult_Release(content);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Get non-existing blob", "[Blob]") {
    CBLError error;
    FLMutableDict blobProps = FLMutableDict_New();
    FLMutableDict_SetString(blobProps, kCBLTypeProperty, kCBLBlobType);
    FLMutableDict_SetString(blobProps, kCBLBlobDigestProperty, "sha1-VVVVVVVVVVVVVVVVVVVVVVVVVVU="_sl);
    ExpectingExceptions x;
    CHECK(!CBLDatabase_GetBlob(db, blobProps, &error));
    CHECK(error.code == 0);
    FLMutableDict_Release(blobProps);
}


TEST_CASE_METHOD(DatabaseTest, "Get blob using invalid properties", "[blob]") {
    CBLError error;
    FLMutableDict blobProps = FLMutableDict_New();
    FLMutableDict_SetString(blobProps, kCBLTypeProperty, kCBLBlobType);
    ExpectingExceptions x;
    CHECK(!CBLDatabase_GetBlob(db, blobProps, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    FLMutableDict_Release(blobProps);
}


TEST_CASE_METHOD(DatabaseTest, "Get blob", "[Blob]") {
    // Create and Save blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    CHECK(CBLDatabase_SaveBlob(db, blob, &error));
    
    // Copy blob properties and release blob:
    FLMutableDict blobProps = FLDict_MutableCopy(CBLBlob_Properties(blob), kFLDefaultCopy);
    CBLBlob_Release(blob);
    
    const CBLBlob* blob2 = CBLDatabase_GetBlob(db, blobProps, &error);
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    CBLBlob_Release(blob2);
    
    // Compact; blob should be deleted as it is not associated with any docs:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    ExpectingExceptions x;
    CHECK(!CBLDatabase_GetBlob(db, blobProps, &error));
    CHECK(error.code == 0);
}


#pragma mark - CLOSE AND DELETE DATABASE:


#ifdef COUCHBASE_ENTERPRISE

TEST_CASE_METHOD(DatabaseTest, "Close Database with Active Replicator") {
    CBLError error;
    REQUIRE(otherDB);
    
    // Start Replicator:
    auto endpoint = CBLEndpoint_CreateWithLocalDB(otherDB);
    CBLReplicatorConfiguration config = {};
    config.database = db;
    config.endpoint = endpoint;
    config.continuous = true;
    auto repl = CBLReplicator_Create(&config, &error);
    REQUIRE(repl);
    CBLReplicator_Start(repl, false);
    
    int count = 0;
    while (count++ < 100) {
        if (CBLReplicator_Status(repl).activity == kCBLReplicatorIdle)
            break;
        this_thread::sleep_for(100ms);
    }
    CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorIdle);
    
    // Close Database:
    CHECK(CBLDatabase_Close(db, &error));
    
    // Check if the replicator is stopped:
    CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorStopped);
    
    CBLEndpoint_Free(endpoint);
    CBLReplicator_Release(repl);
    
    // For async clean up in Replicator:
    this_thread::sleep_for(200ms);
}

TEST_CASE_METHOD(DatabaseTest, "Delete Database with Active Replicator") {
    CBLError error;
    
    // Start Replicator:
    auto endpoint = CBLEndpoint_CreateWithLocalDB(otherDB);
    CBLReplicatorConfiguration config = {};
    config.database = db;
    config.endpoint = endpoint;
    config.continuous = true;
    auto repl = CBLReplicator_Create(&config, &error);
    REQUIRE(repl);
    CBLReplicator_Start(repl, false);
    
    // Wait util the replicator starts to run:
    int count = 0;
    while (count++ < 100) {
        if (CBLReplicator_Status(repl).activity == kCBLReplicatorIdle) {
            break;
        }
        this_thread::sleep_for(100ms);
    }
    CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorIdle);
    
    // Delete Database:
    CHECK(CBLDatabase_Delete(db, &error));
    
    // Check if the replicator is stopped:
    CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorStopped);
    
    CBLEndpoint_Free(endpoint);
    CBLReplicator_Release(repl);

    // For async clean up in Replicator:
    this_thread::sleep_for(200ms);
}

#endif


TEST_CASE_METHOD(DatabaseTest, "Close Database with Active Live Query") {
    ImportJSONLines("names_100.json", defaultCollection);

    CBLError error;
    auto query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                         "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                         nullptr, &error);
    REQUIRE(query);
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        auto newResults = CBLQuery_CopyCurrentResults(query, token, nullptr);
        CHECK(newResults);
        CBLResultSet_Release(newResults);
    }, this);
    REQUIRE(listenerToken);
    
    // Close database:
    CHECK(CBLDatabase_Close(db, &error));
    
    // Cleanup:
    CBLQuery_Release(query);
    CBLListener_Remove(listenerToken);
    
    // Sleeping to ensure async cleanup
    this_thread::sleep_for(400ms);
}


TEST_CASE_METHOD(DatabaseTest, "Delete Database with Active Live Query") {
    ImportJSONLines("names_100.json", defaultCollection);
    
    CBLError error;
    auto query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                         "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                         nullptr, &error);
    REQUIRE(query);
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        auto newResults = CBLQuery_CopyCurrentResults(query, token, nullptr);
        CHECK(newResults);
        CBLResultSet_Release(newResults);
    }, this);
    REQUIRE(listenerToken);
    
    // Delete database:
    CHECK(CBLDatabase_Delete(db, &error));
    
    // Cleanup:
    CBLQuery_Release(query);
    CBLListener_Remove(listenerToken);
    
    // Sleeping to ensure async cleanup
    this_thread::sleep_for(400ms);
}

TEST_CASE_METHOD(DatabaseTest, "Use Closed Database") {
    CBLError error = {};
    CHECK(CBLDatabase_Close(db, &error));
    
    testInvalidDatabase();
}

TEST_CASE_METHOD(DatabaseTest, "Use Deleted Database") {
    CBLError error = {};
    CHECK(CBLDatabase_Delete(db, &error));
    
    testInvalidDatabase();
}
