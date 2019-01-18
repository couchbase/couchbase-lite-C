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
