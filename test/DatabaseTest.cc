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
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <string>

using namespace std;
using namespace fleece;


TEST_CASE_METHOD(CBLTest, "Database") {
    CHECK(string(cbl_db_name(db)) == kDatabaseName);
    CHECK(string(cbl_db_path(db)) == string(kDatabaseDir) + "/" + kDatabaseName + ".cblite2");
    CHECK(cbl_db_count(db) == 0);
    CHECK(cbl_db_lastSequence(db) == 0);
}


TEST_CASE_METHOD(CBLTest, "New Document") {
    CBLDocument* doc = cbl_doc_new("foo");
    CHECK(doc != nullptr);
    CHECK(string(cbl_doc_id(doc)) == "foo");
    CHECK(cbl_doc_sequence(doc) == 0);
    CHECK(string(cbl_doc_propertiesAsJSON(doc)) == "{}");
    CHECK(cbl_doc_mutableProperties(doc) == cbl_doc_properties(doc));
    cbl_doc_release(doc);
}


TEST_CASE_METHOD(CBLTest, "Save Empty Document") {
    CBLDocument* doc = cbl_doc_new("foo");
    CBLError error;
    const CBLDocument *saved = cbl_db_saveDocument(db, doc, kCBLConcurrencyControlFailOnConflict, &error);
    REQUIRE(saved);
    CHECK(string(cbl_doc_id(saved)) == "foo");
    CHECK(cbl_doc_sequence(saved) == 1);
    CHECK(string(cbl_doc_propertiesAsJSON(saved)) == "{}");
    cbl_doc_release(saved);
    cbl_doc_release(doc);

    doc = cbl_db_getMutableDocument(db, "foo");
    CHECK(string(cbl_doc_id(doc)) == "foo");
    CHECK(cbl_doc_sequence(doc) == 1);
    CHECK(string(cbl_doc_propertiesAsJSON(doc)) == "{}");
    cbl_doc_release(doc);
}


TEST_CASE_METHOD(CBLTest, "Save Document With Property") {
    CBLDocument* doc = cbl_doc_new("foo");
    MutableDict props = cbl_doc_mutableProperties(doc);
    props["greeting"_sl] = "Howdy!"_sl;
    // or alternatively:  FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    CHECK(string(cbl_doc_propertiesAsJSON(doc)) == "{\"greeting\":\"Howdy!\"}");
    CHECK(Dict(cbl_doc_properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");

    CBLError error;
    const CBLDocument *saved = cbl_db_saveDocument(db, doc, kCBLConcurrencyControlFailOnConflict, &error);
    REQUIRE(saved);
    CHECK(string(cbl_doc_id(saved)) == "foo");
    CHECK(cbl_doc_sequence(saved) == 1);
    CHECK(string(cbl_doc_propertiesAsJSON(saved)) == "{\"greeting\":\"Howdy!\"}");
    CHECK(Dict(cbl_doc_properties(saved)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    cbl_doc_release(saved);
    cbl_doc_release(doc);

    doc = cbl_db_getMutableDocument(db, "foo");
    CHECK(string(cbl_doc_id(doc)) == "foo");
    CHECK(cbl_doc_sequence(doc) == 1);
    CHECK(string(cbl_doc_propertiesAsJSON(doc)) == "{\"greeting\":\"Howdy!\"}");
    CHECK(Dict(cbl_doc_properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    cbl_doc_release(doc);
}


static void createDocument(CBLDatabase *db, const char *docID,
                           const char *property, const char *value)
{
    CBLDocument* doc = cbl_doc_new(docID);
    MutableDict props = cbl_doc_mutableProperties(doc);
    FLMutableDict_SetString(props, slice(property), slice(value));
    CBLError error;
    const CBLDocument *saved = cbl_db_saveDocument(db, doc, kCBLConcurrencyControlFailOnConflict,
                                                   &error);
    cbl_doc_release(doc);
    REQUIRE(saved);
    cbl_doc_release(saved);
}


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
    auto token = cbl_db_addListener(db, dbListener, this);
    auto docToken = cbl_db_addDocumentListener(db, "foo", fooListener, this);

    // Create a doc, check that the listener was called:
    createDocument(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    cbl_listener_remove(token);
    cbl_listener_remove(docToken);

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
    auto token = cbl_db_addListener(db, dbListener2, this);
    auto fooToken = cbl_db_addDocumentListener(db, "foo", fooListener, this);
    auto barToken = cbl_db_addDocumentListener(db, "bar", barListener, this);
    cbl_db_bufferNotifications(db, notificationsReady, this);

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
    cbl_db_sendNotifications(db);
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    // There should be no more notifications:
    cbl_db_sendNotifications(db);
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    cbl_listener_remove(token);
    cbl_listener_remove(fooToken);
    cbl_listener_remove(barToken);
}
