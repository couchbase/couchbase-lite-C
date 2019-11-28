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


static void createDocument(CBLDatabase *db, const char *docID,
                           const char *property, const char *value)
{
    CBLDocument* doc = CBLDocument_New(docID);
    MutableDict props = CBLDocument_MutableProperties(doc);
    FLSlot_SetString(FLMutableDict_Set(props, slice(property)), slice(value));
    CBLError error;
    const CBLDocument *saved = CBLDatabase_SaveDocument(db, doc, kCBLConcurrencyControlFailOnConflict,
                                                        &error);
    CBLDocument_Release(doc);
    REQUIRE(saved);
    CBLDocument_Release(saved);
}


TEST_CASE_METHOD(CBLTest, "Database") {
    CHECK(string(CBLDatabase_Name(db)) == kDatabaseName);
    CHECK(string(CBLDatabase_Path(db)) == string(kDatabaseDir) + kPathSeparator + kDatabaseName + ".cblite2" + kPathSeparator);
    CHECK(CBL_DatabaseExists(kDatabaseName, kDatabaseDir.c_str()));
    CHECK(CBLDatabase_Count(db) == 0);
    CHECK(CBLDatabase_LastSequence(db) == 0);       // not public API
}


TEST_CASE_METHOD(CBLTest, "Database w/o config") {
    CBLError error;
    CBLDatabase *defaultdb = CBLDatabase_Open("unconfig", nullptr, &error);
    REQUIRE(defaultdb);
    const char * path = CBLDatabase_Path(defaultdb);
    cerr << "Default database is at " << path << "\n";
    CHECK(CBL_DatabaseExists("unconfig", nullptr));

    CBLDatabaseConfiguration config = CBLDatabase_Config(defaultdb);
    CHECK(config.directory != nullptr);     // exact value is platform-specific
    CHECK(config.flags == kCBLDatabase_Create);
    CHECK(config.encryptionKey.algorithm == kCBLEncryptionNone);

    CHECK(CBLDatabase_Delete(defaultdb, &error));
    CBLDatabase_Release(defaultdb);

    CHECK(!CBL_DatabaseExists("unconfig", nullptr));
}


TEST_CASE_METHOD(CBLTest, "New Document") {
    CBLDocument* doc = CBLDocument_New("foo");
    CHECK(doc != nullptr);
    CHECK(string(CBLDocument_ID(doc)) == "foo");
    CHECK(CBLDocument_RevisionID(doc) == nullptr);
    CHECK(CBLDocument_Sequence(doc) == 0);
    CHECK(string(CBLDocument_PropertiesAsJSON(doc)) == "{}");
    CHECK(CBLDocument_MutableProperties(doc) == CBLDocument_Properties(doc));
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(CBLTest, "Save Empty Document") {
    CBLDocument* doc = CBLDocument_New("foo");
    CBLError error;
    const CBLDocument *saved = CBLDatabase_SaveDocument(db, doc, kCBLConcurrencyControlFailOnConflict, &error);
    REQUIRE(saved);
    CHECK(string(CBLDocument_ID(saved)) == "foo");
    CHECK(CBLDocument_Sequence(saved) == 1);
    CHECK(string(CBLDocument_PropertiesAsJSON(saved)) == "{}");
    CBLDocument_Release(saved);
    CBLDocument_Release(doc);

    doc = CBLDatabase_GetMutableDocument(db, "foo");
    CHECK(string(CBLDocument_ID(doc)) == "foo");
    CHECK(string(CBLDocument_RevisionID(doc)) == "1-581ad726ee407c8376fc94aad966051d013893c4");
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(string(CBLDocument_PropertiesAsJSON(doc)) == "{}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(CBLTest, "Save Document With Property") {
    CBLDocument* doc = CBLDocument_New("foo");
    MutableDict props = CBLDocument_MutableProperties(doc);
    props["greeting"_sl] = "Howdy!"_sl;
    // or alternatively:  FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    CHECK(string(CBLDocument_PropertiesAsJSON(doc)) == "{\"greeting\":\"Howdy!\"}");
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");

    CBLError error;
    const CBLDocument *saved = CBLDatabase_SaveDocument(db, doc, kCBLConcurrencyControlFailOnConflict, &error);
    REQUIRE(saved);
    CHECK(string(CBLDocument_ID(saved)) == "foo");
    CHECK(CBLDocument_Sequence(saved) == 1);
    CHECK(string(CBLDocument_PropertiesAsJSON(saved)) == "{\"greeting\":\"Howdy!\"}");
    CHECK(Dict(CBLDocument_Properties(saved)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(saved);
    CBLDocument_Release(doc);

    doc = CBLDatabase_GetMutableDocument(db, "foo");
    CHECK(string(CBLDocument_ID(doc)) == "foo");
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(string(CBLDocument_PropertiesAsJSON(doc)) == "{\"greeting\":\"Howdy!\"}");
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(CBLTest, "Missing document") {
    CBLError error;
    REQUIRE(!CBLDatabase_PurgeDocumentByID(db, "bogus", &error));
    CHECK(error.domain == CBLDomain);
    CHECK(error.code == CBLErrorNotFound);
    CHECK(string(CBLError_Message(&error)) == "not found");
}


TEST_CASE_METHOD(CBLTest, "Expiration") {
    createDocument(db, "doc1", "foo", "bar");
    createDocument(db, "doc2", "foo", "bar");
    createDocument(db, "doc3", "foo", "bar");

    CBLError error;
    CBLTimestamp future = CBL_Now() + 1000;
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc1", future, &error));
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc3", future, &error));
    CHECK(CBLDatabase_Count(db) == 3);

    CHECK(CBLDatabase_GetDocumentExpiration(db, "doc1", &error) == future);
    CHECK(CBLDatabase_GetDocumentExpiration(db, "doc2", &error) == 0);
    CHECK(CBLDatabase_GetDocumentExpiration(db, "docX", &error) == 0);

    this_thread::sleep_for(chrono::milliseconds(1700));
    CHECK(CBLDatabase_Count(db) == 1);
}


#pragma mark - LISTENERS:


static int dbListenerCalls = 0;
static int fooListenerCalls = 0;

static void dbListener(void *context, const CBLDatabase *db, unsigned nDocs, const char** docIDs) {
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 1);
    CHECK(string(docIDs[0]) == "foo");
}

static void fooListener(void *context, const CBLDatabase *db, const char *docID) {
    ++fooListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(string(docID) == "foo");
}


TEST_CASE_METHOD(CBLTest, "Database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener, this);
    auto docToken = CBLDatabase_AddDocumentChangeListener(db, "foo", fooListener, this);

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

static void dbListener2(void *context, const CBLDatabase *db, unsigned nDocs, const char** docIDs) {
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 2);
    CHECK(string(docIDs[0]) == "foo");
    CHECK(string(docIDs[1]) == "bar");
}

int barListenerCalls = 0;

static void barListener(void *context, const CBLDatabase *db, const char *docID) {
    ++barListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(string(docID) == "bar");
}


TEST_CASE_METHOD(CBLTest, "Scheduled database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = barListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener2, this);
    auto fooToken = CBLDatabase_AddDocumentChangeListener(db, "foo", fooListener, this);
    auto barToken = CBLDatabase_AddDocumentChangeListener(db, "bar", barListener, this);
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
