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
    CHECK(db.path() == string(kDatabaseDir) + kPathSeparator + string(kDatabaseName) + ".cblite2" + kPathSeparator);
    CHECK(db.count() == 0);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Database Exist") {
    CHECK(!Database::exists(kDatabaseName, nullptr));
    CHECK(Database::exists(kDatabaseName, kDatabaseDir));
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Copy Database") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);
    
    const slice copiedDBName = "CBLtest_Copied";
    Database::deleteDatabase(copiedDBName, kDatabaseDir);
    REQUIRE(!Database::exists(copiedDBName, kDatabaseDir));
    
    Database::copyDatabase(db.path(), copiedDBName, CBLTest::kDatabaseConfiguration);
    
    CHECK(Database::exists(copiedDBName, kDatabaseDir));
    auto copiedDB = cbl::Database(copiedDBName, CBLTest::kDatabaseConfiguration);
    CHECK(copiedDB.count() == 1);
    
    doc = copiedDB.getMutableDocument("foo");
    CHECK(doc["greeting"].asString() == "Howdy!");
    
    copiedDB.deleteDatabase();
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Missing Document") {
    Document doc = db.getDocument("foo");
    CHECK(!doc);

    MutableDocument mdoc = db.getMutableDocument("foo");
    CHECK(!mdoc);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Save Empty Document") {
    MutableDocument doc("foo");
    db.saveDocument(doc);
    CHECK(string(doc.id()) == "foo");
    CHECK(doc.sequence() == 1);
    CHECK(!doc.revisionID().empty());
    CHECK(doc.properties().toJSONString() == "{}");

    MutableDocument doc2 = db.getMutableDocument("foo");
    CHECK(string(doc2.id()) == "foo");
    CHECK(doc2.sequence() == 1);
    CHECK(doc2.revisionID() == doc.revisionID());
    CHECK(doc2.properties().toJSONString() == "{}");
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Save Document With Property") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc["greeting"].asString() == "Howdy!");
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");

    db.saveDocument(doc);
    CHECK(string(doc.id()) == "foo");
    CHECK(doc.sequence() == 1);
    CHECK(!doc.revisionID().empty());
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CHECK(doc["greeting"].asString() == "Howdy!");

    MutableDocument doc2 = db.getMutableDocument("foo");
    CHECK(string(doc2.id()) == "foo");
    CHECK(doc2.sequence() == 1);
    CHECK(doc2.revisionID() == doc.revisionID());
    CHECK(doc2.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CHECK(doc2["greeting"].asString() == "Howdy!");
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Delete Unsaved Doc") {
    MutableDocument doc("foo");
    ExpectingExceptions x;
    CBLError error;
    REQUIRE(!CBLDatabase_DeleteDocumentWithConcurrencyControl(db.ref(), doc.ref(), kCBLConcurrencyControlLastWriteWins, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Purge Doc") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);
    
    doc = db.getMutableDocument("foo");
    REQUIRE(doc);
    
    SECTION("Purge with Doc") {
        db.purgeDocument(doc);
    }
    
    SECTION("Purge with ID") {
        db.purgeDocument("foo");
    }
    
    doc = db.getMutableDocument("foo");
    CHECK(!doc);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Document Expiration") {
    MutableDocument doc1("doc1");
    db.saveDocument(doc1);
    
    MutableDocument doc2("doc2");
    db.saveDocument(doc2);
    
    MutableDocument doc3("doc3");
    db.saveDocument(doc3);
    
    CBLTimestamp future = CBL_Now() + 1000;
    db.setDocumentExpiration("doc1", future);
    db.setDocumentExpiration("doc3", future);
    
    CHECK(db.count() == 3);
    CHECK(db.getDocumentExpiration("doc1") == future);
    CHECK(db.getDocumentExpiration("doc3") == future);
    CHECK(db.getDocumentExpiration("doc2") == 0);
    CHECK(db.getDocumentExpiration("docx") == 0);

    this_thread::sleep_for(2000ms);
    CHECK(db.count() == 1);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Transaction") {
    {
        Transaction t(db);

        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        db.saveDocument(doc);
        doc["meeting"] = 23;
        db.saveDocument(doc);

        t.commit();
    }

    Document checkDoc = db.getDocument("foo");
    REQUIRE(checkDoc);
    CHECK(checkDoc.properties().get("greeting").asString() == "Howdy!");
    CHECK(checkDoc.properties().get("meeting").asInt() == 23);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Transaction, Abort") {
    {
        Transaction t(db);

        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        db.saveDocument(doc);
        doc["meeting"] = 23;
        db.saveDocument(doc);
        
        SECTION("No commit abort") { }
        
        SECTION("Explicit abort") {
            t.abort();
        }
    }

    Document checkDoc = db.getDocument("foo");
    REQUIRE(!checkDoc);
}

TEST_CASE_METHOD(CBLTest_Cpp, "C++ Transaction With Exception", "[!throws]") {
    {
        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        db.saveDocument(doc);
    }

    bool threw = false;
    try {
        Transaction t(db);

        MutableDocument doc("foo");
        doc["meeting"] = 23;
        db.saveDocument(doc);

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

    Document doc = db.getDocument("foo");
    REQUIRE(doc);
    CHECK(doc["greeting"].asString() == "Howdy!");
    CHECK(doc["meeting"] == nullptr);
}

static void createDocument(Database db, const char *docID,
                           const char *property, const char *value)
{
    MutableDocument doc(docID);
    doc[property] = value;
    db.saveDocument(doc);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Database notifications") {
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
        createDocument(db, "foo", "greeting", "Howdy!");
        CHECK(dbListenerCalls == 1);
        CHECK(fooListenerCalls == 1);
    }
    // After being removed, the listener should not be called:
    dbListenerCalls = fooListenerCalls = 0;
    createDocument(db, "bar", "greeting", "yo.");
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
    createDocument(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    createDocument(db, "bar", "greeting", "yo.");
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

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - C++ Save Conflict") {
    MutableDocument doc("foo");
    doc["n"] = 10;
    db.saveDocument(doc);

    MutableDocument shadowDoc = db.getMutableDocument("foo");
    shadowDoc["n"] = 7;
    db.saveDocument(shadowDoc);

    doc["n"] = 11;
    REQUIRE(!db.saveDocument(doc, kCBLConcurrencyControlFailOnConflict));
    REQUIRE(db.saveDocument(doc, kCBLConcurrencyControlLastWriteWins));

    shadowDoc["n"] = 8;
    bool result = db.saveDocument(shadowDoc, [&](MutableDocument myDoc, Document otherDoc) {
        CHECK(myDoc["n"].asInt() == 8);
        CHECK(otherDoc["n"].asInt() == 11);
        myDoc["n"] = 19;
        return true;
    });
    CHECK(result);
    CHECK(shadowDoc["n"].asInt() == 19);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Legacy - Create and Delete Index") {
    RetainedArray names = db.getIndexNames();
    REQUIRE(names);
    REQUIRE(names.count() == 0);
    
    CBLValueIndexConfiguration index1 = {kCBLN1QLLanguage, "id"_sl};
    db.createValueIndex("index1", index1);
    
    CBLValueIndexConfiguration index2 = {kCBLN1QLLanguage, "firstname, lastname"_sl};
    db.createValueIndex("index2", index2);
    
    CBLFullTextIndexConfiguration index3 = {kCBLN1QLLanguage, "product.description"_sl, true};
    db.createFullTextIndex("index3", index3);
    
    names = db.getIndexNames();
    REQUIRE(names);
    REQUIRE(names.count() == 3);
    CHECK(names[0].asString() == "index1");
    CHECK(names[1].asString() == "index2");
    CHECK(names[2].asString() == "index3");
    
    db.deleteIndex("index1");
    db.deleteIndex("index3");
    
    names = db.getIndexNames();
    REQUIRE(names);
    REQUIRE(names.count() == 1);
    CHECK(names[0].asString() == "index2");
}

TEST_CASE_METHOD(CBLTest_Cpp, "Add new key") {
    // Regression test for <https://github.com/couchbaselabs/couchbase-lite-C/issues/18>
    // Add doc to db:
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);

    // Add a new, shareable key:
    doc.set("new", 10);
    db.saveDocument(doc);

    CHECK(doc["new"].asInt() == 10);
    doc["new"] = 999;
    CHECK(doc["new"].asInt() == 999);
    CHECK(doc.properties().count() == 2);

    doc = db.getMutableDocument("foo");
    CHECK(doc["new"].asInt() == 10);
}

TEST_CASE_METHOD(CBLTest_Cpp, "Data disappears") {
    // Regression test for <https://github.com/couchbaselabs/couchbase-lite-C/issues/19>
    MutableDocument doc = MutableDocument("foo");
    doc["var1"]= 1;
    db.saveDocument(doc);
    CHECK(doc.properties().toJSONString() == "{\"var1\":1}");

    doc = db.getMutableDocument("foo");
    doc["var2"]= 2;
    db.saveDocument(doc);
    CHECK(doc.properties().toJSONString() == "{\"var1\":1,\"var2\":2}");

    doc = db.getMutableDocument("foo");
    doc["var3"]= 3;
    db.saveDocument(doc);
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
    db.saveDocument(mdoc);
    CHECK(mdoc.propertiesAsJSON() == R"({"BAR":"Wahooma","FOO":18})");
    auto savedDoc = db.getDocument("ubiq");
    CHECK(savedDoc.propertiesAsJSON() == mdoc.propertiesAsJSON());
}
