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
#include <string>
#include <thread>

using namespace std;
using namespace fleece;


static void createDocument(CBLDatabase *db, slice docID, slice property, slice value) {
    CBLDocument* doc = CBLDocument_NewWithID(docID);
    MutableDict props = CBLDocument_MutableProperties(doc);
    FLSlot_SetString(FLMutableDict_Set(props, property), value);
    CBLError error;
    bool saved = CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlFailOnConflict, &error);
    CBLDocument_Release(doc);
    REQUIRE(saved);
}


TEST_CASE_METHOD(CBLTest, "Database") {
    CHECK(CBLDatabase_Name(db) == kDatabaseName);
    CHECK(string(CBLDatabase_Path(db)) == string(kDatabaseDir) + kPathSeparator + string(kDatabaseName) + ".cblite2" + kPathSeparator);
    CHECK(CBL_DatabaseExists(kDatabaseName, kDatabaseDir));
    CHECK(CBLDatabase_Count(db) == 0);
    CHECK(CBLDatabase_LastSequence(db) == 0);       // not public API
}


TEST_CASE_METHOD(CBLTest, "Database w/o config") {
    CBLError error;
    CBLDatabase *defaultdb = CBLDatabase_Open("unconfig"_sl, nullptr, &error);
    REQUIRE(defaultdb);
    alloc_slice path = CBLDatabase_Path(defaultdb);
    cerr << "Default database is at " << path << "\n";
    CHECK(CBL_DatabaseExists("unconfig"_sl, nullslice));

    CBLDatabaseConfiguration config = CBLDatabase_Config(defaultdb);
    CHECK(config.directory != nullslice);     // exact value is platform-specific
    CHECK(config.flags == kCBLDatabase_Create);
    CHECK(config.encryptionKey == nullptr);

    CHECK(CBLDatabase_Delete(defaultdb, &error));
    CBLDatabase_Release(defaultdb);

    CHECK(!CBL_DatabaseExists("unconfig"_sl, nullslice));
}


TEST_CASE_METHOD(CBLTest, "Missing Document") {
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "foo"_sl, &error);
    CHECK(doc == nullptr);
    CHECK(error.domain == CBLDomain);
    CHECK(error.code == CBLErrorNotFound);

    CBLDocument* mdoc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(mdoc == nullptr);
    CHECK(error.domain == CBLDomain);
    CHECK(error.code == CBLErrorNotFound);

    CHECK(!CBLDatabase_PurgeDocumentByID(db, "foo"_sl, &error));
    CHECK(error.domain == CBLDomain);
    CHECK(error.code == CBLErrorNotFound);
}


TEST_CASE_METHOD(CBLTest, "New Document") {
    CBLDocument* doc = CBLDocument_NewWithID("foo"_sl);
    CHECK(doc != nullptr);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_RevisionID(doc) == nullslice);
    CHECK(CBLDocument_Sequence(doc) == 0);
    CHECK(CBLDocument_ToJSON(doc) == "{}"_sl);
    CHECK(CBLDocument_MutableProperties(doc) == CBLDocument_Properties(doc));
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(CBLTest, "Save Empty Document") {
    CBLDocument* doc = CBLDocument_NewWithID("foo"_sl);
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlFailOnConflict, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_ToJSON(doc)) == "{}"_sl);
    CBLDocument_Release(doc);

    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_RevisionID(doc) == "1-581ad726ee407c8376fc94aad966051d013893c4"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_ToJSON(doc)) == "{}"_sl);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(CBLTest, "Save Document With Property") {
    CBLDocument* doc = CBLDocument_NewWithID("foo"_sl);
    MutableDict props = CBLDocument_MutableProperties(doc);
    props["greeting"_sl] = "Howdy!"_sl;
    // or alternatively:  FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    CHECK(alloc_slice(CBLDocument_ToJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");

    CBLError error;
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlFailOnConflict, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_ToJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);

    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_ToJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(CBLTest, "Missing document") {
    CBLError error;
    REQUIRE(!CBLDatabase_PurgeDocumentByID(db, "bogus"_sl, &error));
    CHECK(error.domain == CBLDomain);
    CHECK(error.code == CBLErrorNotFound);
    CHECK(alloc_slice(CBLError_Message(&error)) == "not found"_sl);
}


TEST_CASE_METHOD(CBLTest, "Expiration") {
    createDocument(db, "doc1", "foo", "bar");
    createDocument(db, "doc2", "foo", "bar");
    createDocument(db, "doc3", "foo", "bar");

    CBLError error;
    CBLTimestamp future = CBL_Now() + 1000;
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc1"_sl, future, &error));
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc3"_sl, future, &error));
    CHECK(CBLDatabase_Count(db) == 3);

    CHECK(CBLDatabase_GetDocumentExpiration(db, "doc1"_sl, &error) == future);
    CHECK(CBLDatabase_GetDocumentExpiration(db, "doc2"_sl, &error) == 0);
    CHECK(CBLDatabase_GetDocumentExpiration(db, "docX"_sl, &error) == 0);

    this_thread::sleep_for(1700ms);
    CHECK(CBLDatabase_Count(db) == 1);
}


TEST_CASE_METHOD(CBLTest, "Expiration After Reopen") {
    createDocument(db, "doc1", "foo", "bar");
    createDocument(db, "doc2", "foo", "bar");
    createDocument(db, "doc3", "foo", "bar");

    CBLError error;
    CBLTimestamp future = CBL_Now() + 2000;
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc1"_sl, future, &error));
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc3"_sl, future, &error));
    CHECK(CBLDatabase_Count(db) == 3);

    // Close & reopen the database:
    REQUIRE(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    db = CBLDatabase_Open(kDatabaseName, &kDatabaseConfiguration, &error);

    // Now wait for expiration:
    this_thread::sleep_for(3000ms);
    CHECK(CBLDatabase_Count(db) == 1);
}


TEST_CASE_METHOD(CBLTest, "Maintenance : Compact and Integrity Check") {
    // Create a doc with blob:
    CBLDocument* doc = CBLDocument_NewWithID("doc1"_sl);
    FLMutableDict dict = CBLDocument_MutableProperties(doc);
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob *blob1 = CBLBlob_NewWithData("text/plain"_sl, blobContent);
    FLSlot_SetBlob(FLMutableDict_Set(dict, FLStr("blob")), blob1);
    
    // Save doc:
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLBlob_Release(blob1);
    CBLDocument_Release(doc);
    
    // Compact:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    
    // Make sure the blob still exists after compact: (issue #73)
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(CBLDocument_Properties(doc), FLStr("blob")));
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    FLSliceResult_Release(content);
    
    // https://issues.couchbase.com/browse/CBL-1617
    // CBLBlob_Release(blob2);
    
    // Delete doc:
    CHECK(CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLDocument_Release(doc);
    
    // Compact:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    
    // Integrity check:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeIntegrityCheck, &error));
}


TEST_CASE_METHOD(CBLTest, "Maintenance : Reindex") {
    CBLError error;
    CBLIndexSpec index = {};
    index.type = kCBLValueIndex;
    index.keyExpressionsJSON = R"(["foo"])"_sl;
    CHECK(CBLDatabase_CreateIndex(db, "foo"_sl, index, &error));
    
    createDocument(db, "doc1", "foo", "bar1");
    createDocument(db, "doc2", "foo", "bar2");
    createDocument(db, "doc3", "foo", "bar3");
    
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeReindex, &error));
    
    FLMutableArray names = CBLDatabase_IndexNames(db);
    REQUIRE(names);
    CHECK(FLArray_Count(names) == 1);
    CHECK(FLValue_AsString(FLArray_Get(names, 0)) == "foo"_sl);
    FLMutableArray_Release(names);
}


#pragma mark - LISTENERS:


static int dbListenerCalls = 0;
static int fooListenerCalls = 0;

static void dbListener(void *context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs) {
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 1);
    CHECK(slice(docIDs[0]) == "foo"_sl);
}

static void fooListener(void *context, const CBLDatabase *db, FLString docID) {
    ++fooListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(slice(docID) == "foo"_sl);
}


TEST_CASE_METHOD(CBLTest, "Database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener, this);
    auto docToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);

    // Create a doc, check that the listener was called:
    createDocument(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    CBLListener_Remove(token);
    CBLListener_Remove(docToken);

    // After being removed, the listener should not be called:
    dbListenerCalls = fooListenerCalls = 0;
    createDocument(db, "bar", "greeting", "yo.");
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
}


static int notificationsReadyCalls = 0;

static void notificationsReady(void *context, CBLDatabase* db) {
    ++notificationsReadyCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
}

static void dbListener2(void *context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs) {
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 2);
    CHECK(docIDs[0] == "foo"_sl);
    CHECK(docIDs[1] == "bar"_sl);
}

int barListenerCalls = 0;

static void barListener(void *context, const CBLDatabase *db, FLString docID) {
    ++barListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(docID == "bar"_sl);
}


TEST_CASE_METHOD(CBLTest, "Scheduled database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = barListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener2, this);
    auto fooToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);
    auto barToken = CBLDatabase_AddDocumentChangeListener(db, "bar"_sl, barListener, this);
    CBLDatabase_BufferNotifications(db, notificationsReady, this);

    // Create two docs; no listeners should be called yet:
    createDocument(db, "foo", "greeting", "Howdy!");
    CHECK(notificationsReadyCalls == 1);
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    createDocument(db, "bar", "greeting", "yo.");
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
