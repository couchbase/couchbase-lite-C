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

#include "CBLTest.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <string>

#include "CouchbaseLite.hh"

using namespace std;
using namespace fleece;
using namespace cbl;


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Database") {
    CHECK(string(db.name()) == CBLTest::kDatabaseName);
    CHECK(string(db.path()) == string(kDatabaseDir) + "/" + kDatabaseName + ".cblite2");
    CHECK(db.count() == 0);
    CHECK(db.lastSequence() == 0);
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ New Document") {
    MutableDocument doc("foo");
    CHECK(doc);
    CHECK(string(doc.id()) == "foo");
    CHECK(doc.sequence() == 0);
    CHECK(doc.properties().toJSONString() == "{}");

    Document immDoc = doc;
    CHECK((doc.properties() == immDoc.properties()));
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Save Empty Document") {
    MutableDocument doc("foo");
    Document saved = db.saveDocument(doc);
    REQUIRE(saved);
    CHECK(string(saved.id()) == "foo");
    CHECK(saved.sequence() == 1);
    CHECK(saved.properties().toJSONString() == "{}");

    doc = db.getMutableDocument("foo");
    CHECK(string(doc.id()) == "foo");
    CHECK(doc.sequence() == 1);
    CHECK(doc.properties().toJSONString() == "{}");
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Save Document With Property") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc["greeting"].asString() == "Howdy!"_sl);
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");

    auto saved = db.saveDocument(doc);
    REQUIRE(saved);
    CHECK(string(saved.id()) == "foo");
    CHECK(saved.sequence() == 1);
    CHECK(saved.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CHECK(saved["greeting"].asString() == "Howdy!"_sl);

    doc = db.getMutableDocument("foo");
    CHECK(string(doc.id()) == "foo");
    CHECK(doc.sequence() == 1);
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CHECK(doc["greeting"].asString() == "Howdy!"_sl);
}
