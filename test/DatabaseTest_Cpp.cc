//
// DatabaseTest_Cpp.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "CBLTest_Cpp.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <string>
#include <thread>

#include "cbl++/CouchbaseLite.hh"

using namespace std;
using namespace fleece;
using namespace cbl;


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Database") {
    CHECK(db.name() == string(kDatabaseName));
    CHECK(db.path() == string(CBLTest::databaseDir()) + kPathSeparator + string(kDatabaseName) + ".cblite2" + kPathSeparator);
    CHECK(db.count() == 0);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Database Exist") {
    CHECK(!Database::exists(kDatabaseName, nullptr));
    CHECK(Database::exists(kDatabaseName, CBLTest::databaseDir()));
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Copy Database") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    defaultCollection.saveDocument(doc);

    auto dbDir = CBLTest::databaseDir();
    auto config = CBLTest::databaseConfig();
    
    const slice copiedDBName = "CBLtest_Copied";
    Database::deleteDatabase(copiedDBName, dbDir);
    REQUIRE(!Database::exists(copiedDBName, dbDir));
    
    Database::copyDatabase(db.path(), copiedDBName, config);
    
    CHECK(Database::exists(copiedDBName, dbDir));
    auto copiedDB = cbl::Database(copiedDBName, config);
    CHECK(copiedDB.count() == 1);
    
    doc = copiedDB.getMutableDocument("foo");
    CHECK(doc["greeting"].asString() == "Howdy!");
    
    copiedDB.deleteDatabase();
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Save Document With Property") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc["greeting"].asString() == "Howdy!");
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");

    defaultCollection.saveDocument(doc);
    CHECK(string(doc.id()) == "foo");
    CHECK(doc.sequence() == 1);
    CHECK(!doc.revisionID().empty());
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CHECK(doc["greeting"].asString() == "Howdy!");

    MutableDocument doc2 = defaultCollection.getMutableDocument("foo");
    CHECK(string(doc2.id()) == "foo");
    CHECK(doc2.sequence() == 1);
    CHECK(doc2.revisionID() == doc.revisionID());
    CHECK(doc2.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CHECK(doc2["greeting"].asString() == "Howdy!");
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Delete Unsaved Doc") {
    MutableDocument doc("foo");
    ExpectingExceptions x;
    CBLError error;
    REQUIRE(!CBLCollection_DeleteDocumentWithConcurrencyControl(defaultCollection.ref(), doc.ref(), kCBLConcurrencyControlLastWriteWins, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Transaction") {
    {
        Transaction t(db);

        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        defaultCollection.saveDocument(doc);
        doc["meeting"] = 23;
        defaultCollection.saveDocument(doc);

        t.commit();
    }

    Document checkDoc = defaultCollection.getDocument("foo");
    REQUIRE(checkDoc);
    CHECK(checkDoc.properties().get("greeting").asString() == "Howdy!");
    CHECK(checkDoc.properties().get("meeting").asInt() == 23);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Transaction, Abort") {
    {
        Transaction t(db);

        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        defaultCollection.saveDocument(doc);
        doc["meeting"] = 23;
        defaultCollection.saveDocument(doc);
        
        SECTION("No commit abort") { }
        
        SECTION("Explicit abort") {
            t.abort();
        }
    }

    Document checkDoc = defaultCollection.getDocument("foo");
    REQUIRE(!checkDoc);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Transaction With Exception", "[!throws]") {
    {
        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        defaultCollection.saveDocument(doc);
    }

    bool threw = false;
    try {
        Transaction t(db);

        MutableDocument doc("foo");
        doc["meeting"] = 23;
        defaultCollection.saveDocument(doc);

        if (sqrt(2) > 1.0) {
            ExpectingExceptions x;
            CBL_Log(kCBLLogDomainDatabase, kCBLLogWarning, "INTENTIONALLY THROWING EXCEPTION!");
            throw runtime_error("intentional");
        }

        t.commit();

    } catch (runtime_error &x) {
        threw = true;
        CHECK(string(x.what()) == "intentional");
    }
    CHECK(threw);

    Document doc = defaultCollection.getDocument("foo");
    REQUIRE(doc);
    CHECK(doc["greeting"].asString() == "Howdy!");
    CHECK(doc["meeting"] == nullptr);
}

// db utility function using default collection - collection api
static void createDocumentInDefault(Database db, const char *docID,
                           const char *property, const char *value)
{
    Collection col = db.getDefaultCollection();
    MutableDocument doc(docID);
    doc[property] = value;
    col.saveDocument(doc);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Database notifications") {
    int dbListenerCalls = 0, fooListenerCalls = 0;
    {
        // Add a listener:
        auto dbListener = db.addChangeListener([&](Database callbackdb, vector<slice> docIDs) {
            ++dbListenerCalls;
            CHECK(callbackdb == db);
            CHECK(docIDs.size() == 1);
            CHECK(docIDs[0] == "foo");
        });
        auto fooListener = db.addDocumentChangeListener("foo", [&](Database callbackdb, slice docID) {
            ++fooListenerCalls;
            CHECK(callbackdb == db);
            CHECK(docID == "foo");
        });
        // Create a doc, check that the listener was called:
        createDocumentInDefault(db, "foo", "greeting", "Howdy!");
        CHECK(dbListenerCalls == 1);
        CHECK(fooListenerCalls == 1);
    }
    // After being removed, the listener should not be called:
    dbListenerCalls = fooListenerCalls = 0;
    createDocumentInDefault(db, "bar", "greeting", "yo.");
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Scheduled database notifications") {
    // Add a listener:
    int dbListenerCalls = 0, fooListenerCalls = 0, barListenerCalls = 0, notificationsReadyCalls = 0;
    auto dbListener = db.addChangeListener([&](Database callbackdb, vector<slice> docIDs) {
        ++dbListenerCalls;
        CHECK(callbackdb == db);
        CHECK(docIDs.size() == 2);
        CHECK(docIDs[0] == "foo");
        CHECK(docIDs[1] == "bar");
    });
    auto fooListener = db.addDocumentChangeListener("foo", [&](Database callbackdb, slice docID) {
        ++fooListenerCalls;
        CHECK(callbackdb == db);
        CHECK(docID == "foo");
    });
    auto barListener = db.addDocumentChangeListener("bar", [&](Database callbackdb, slice docID) {
        ++barListenerCalls;
        CHECK(callbackdb == db);
        CHECK(docID == "bar");
    });

    db.bufferNotifications([&](Database callbackdb) {
        ++notificationsReadyCalls;
        CHECK(callbackdb == db);
    });

    // Create two docs; no listeners should be called yet:
    createDocumentInDefault(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    createDocumentInDefault(db, "bar", "greeting", "yo.");
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    // Now the listeners will be called:
    db.sendNotifications();
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    // There should be no more notifications:
    db.sendNotifications();
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Save Conflict") {
    MutableDocument doc("foo");
    doc["n"] = 10;
    defaultCollection.saveDocument(doc);

    MutableDocument shadowDoc = defaultCollection.getMutableDocument("foo");
    shadowDoc["n"] = 7;
    defaultCollection.saveDocument(shadowDoc);

    doc["n"] = 11;
    REQUIRE(!defaultCollection.saveDocument(doc, kCBLConcurrencyControlFailOnConflict));
    REQUIRE(defaultCollection.saveDocument(doc, kCBLConcurrencyControlLastWriteWins));

    shadowDoc["n"] = 8;
    bool result = defaultCollection.saveDocument(shadowDoc, [&](MutableDocument myDoc, Document otherDoc) {
        CHECK(myDoc["n"].asInt() == 8);
        CHECK(otherDoc["n"].asInt() == 11);
        myDoc["n"] = 19;
        return true;
    });
    CHECK(result);
    CHECK(shadowDoc["n"].asInt() == 19);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Create and Delete Index") {
    RetainedArray names = defaultCollection.getIndexNames();
    REQUIRE(names);
    REQUIRE(names.count() == 0);
    
    CBLValueIndexConfiguration index1 = {kCBLN1QLLanguage, "id"_sl};
    defaultCollection.createValueIndex("index1", index1);
    
    CBLValueIndexConfiguration index2 = {kCBLN1QLLanguage, "firstname, lastname"_sl};
    defaultCollection.createValueIndex("index2", index2);
    
    CBLFullTextIndexConfiguration index3 = {kCBLN1QLLanguage, "product.description"_sl, true};
    defaultCollection.createFullTextIndex("index3", index3);
    
    names = defaultCollection.getIndexNames();
    REQUIRE(names);
    REQUIRE(names.count() == 3);
    CHECK(names[0].asString() == "index1");
    CHECK(names[1].asString() == "index2");
    CHECK(names[2].asString() == "index3");
    
    defaultCollection.deleteIndex("index1");
    defaultCollection.deleteIndex("index3");
    
    names = defaultCollection.getIndexNames();
    REQUIRE(names);
    REQUIRE(names.count() == 1);
    CHECK(names[0].asString() == "index2");
}

TEST_CASE_METHOD(CBLTest_Cpp, "Add new key") {
    // Regression test for <https://github.com/couchbaselabs/couchbase-lite-C/issues/18>
    // Add doc to col:
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    defaultCollection.saveDocument(doc);

    // Add a new, shareable key:
    doc.set("new", 10);
    defaultCollection.saveDocument(doc);

    CHECK(doc["new"].asInt() == 10);
    doc["new"] = 999;
    CHECK(doc["new"].asInt() == 999);
    CHECK(doc.properties().count() == 2);

    doc = defaultCollection.getMutableDocument("foo");
    CHECK(doc["new"].asInt() == 10);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Data disappears") {
    // Regression test for <https://github.com/couchbaselabs/couchbase-lite-C/issues/19>
    MutableDocument doc = MutableDocument("foo");
    doc["var1"]= 1;
    defaultCollection.saveDocument(doc);
    CHECK(doc.properties().toJSONString() == "{\"var1\":1}");

    doc = defaultCollection.getMutableDocument("foo");
    doc["var2"]= 2;
    defaultCollection.saveDocument(doc);
    CHECK(doc.properties().toJSONString() == "{\"var1\":1,\"var2\":2}");

    doc = defaultCollection.getMutableDocument("foo");
    doc["var3"]= 3;
    defaultCollection.saveDocument(doc);
    CHECK(doc.properties().toJSONString() == "{\"var1\":1,\"var2\":2,\"var3\":3}");
}

TEST_CASE_METHOD(CBLTest_Cpp, "Retaining immutable Fleece") {
    MutableDocument mdoc("ubiq");
    {
        auto fldoc = fleece::Doc::fromJSON(R"({"msg":{"FOO":18,"BAR":"Wahooma"}})");
        REQUIRE(fldoc);
        Dict message = fldoc["msg"].asDict();
        REQUIRE(message);
        mdoc.setProperties(message);
        // Now the variable `fldoc` goes out of scope, but its data needs to remain valid,
        // since `doc` points into it. The Doc object is retained by the MutableDict in `mdoc`,
        // keeping it alive.
    }
    CHECK(mdoc["FOO"].asInt() == 18);
    CHECK(mdoc["BAR"].asString() == "Wahooma");
    defaultCollection.saveDocument(mdoc);
    CHECK(mdoc.propertiesAsJSON() == R"({"BAR":"Wahooma","FOO":18})");
    auto savedDoc = defaultCollection.getDocument("ubiq");
    CHECK(savedDoc.propertiesAsJSON() == mdoc.propertiesAsJSON());
}

TEST_CASE_METHOD(CBLTest_Cpp, "Empty Listener Token") {
    ListenerToken<> listenerToken;
    CHECK(!listenerToken.context());
    CHECK(!listenerToken.token());
    listenerToken.remove(); // Noops
}

TEST_CASE_METHOD(CBLTest_Cpp, "Listener Token") {
    int num = 0;
    auto cb = [&num]() { num++; };
    ListenerToken<> listenerToken = ListenerToken<>(cb);
    
    // Context / Callback:
    REQUIRE(listenerToken.context());
    (*(ListenerToken<>::Callback*)listenerToken.context())();
    CHECK(num == 1);
    
    // Token:
    CHECK(!listenerToken.token());
    auto dummy = [](void* context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs){ };
    auto listener = CBLDatabase_AddChangeListener(db.ref(), dummy, nullptr);
    listenerToken.setToken(listener);
    CHECK(listenerToken.token() == listener);
    
    // Move Constructor:
    ListenerToken<> listenerToken2 = move(listenerToken);
    REQUIRE(listenerToken2.context());
    (*(ListenerToken<>::Callback*)listenerToken2.context())();
    CHECK(num == 2);
    CHECK(listenerToken2.token() == listener);
    
#ifndef __clang_analyzer__ // Exclude the code from being compiled for analyzer
    CHECK(!listenerToken.context());
    CHECK(!listenerToken.token());
    listenerToken.remove(); // Noops
#endif
    
    // Move Assignment:
    listenerToken = move(listenerToken2);
    REQUIRE(listenerToken.context());
    (*(ListenerToken<>::Callback*)listenerToken.context())();
    CHECK(num == 3);
    CHECK(listenerToken.token() == listener);
    
#ifndef __clang_analyzer__ // Exclude the code from being compiled for analyzer
    CHECK(!listenerToken2.context());
    CHECK(!listenerToken2.token());
    listenerToken2.remove(); // Noops
#endif
    
    // Remove:
    listenerToken.remove();
    CHECK(!listenerToken.context());
    CHECK(!listenerToken.context());
    listenerToken.remove(); // Noops
}
