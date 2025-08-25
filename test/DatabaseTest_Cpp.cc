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
    Collection copiedCol = copiedDB.getDefaultCollection();
    CHECK(copiedCol);
    CHECK(copiedCol.count() == 1);
    
    doc = copiedCol.getMutableDocument("foo");
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
