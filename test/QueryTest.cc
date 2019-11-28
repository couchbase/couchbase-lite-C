//
// QueryTest.cc
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
#include "cbl/CouchbaseLite.h"
#include <iostream>
#include <thread>

using namespace std;
using namespace fleece;
using namespace cbl;


class QueryTest : public CBLTest_Cpp {
public:
    QueryTest() {
        ImportJSONLines(GetTestFilePath("names_100.json"), db.ref());
    }

    ~QueryTest() {
        CBLResultSet_Release(results);
        CBLListener_Remove(listenerToken);
        CBLQuery_Release(query);
    }

    CBLQuery *query =nullptr;
    CBLResultSet *results =nullptr;
    CBLListenerToken *listenerToken =nullptr;
    int resultCount =-1;
};


TEST_CASE_METHOD(QueryTest, "Invalid Query", "[!throws]") {
    CBLError error;
    int errPos;
    query = CBLQuery_New(db.ref(), kCBLN1QLLanguage,
                         "SELECT name WHERE",
                         &errPos, &error);
    REQUIRE(!query);
    CHECK(errPos == 17);
    CHECK(error.domain == CBLDomain);
    CHECK(error.code == CBLErrorInvalidQuery);
}


TEST_CASE_METHOD(QueryTest, "Query") {
    CBLError error;
    int errPos;
    query = CBLQuery_New(db.ref(), kCBLN1QLLanguage,
                         "SELECT name WHERE birthday like '1959-%' ORDER BY birthday",
                         &errPos, &error);
    REQUIRE(query);

    CHECK(CBLQuery_ColumnCount(query) == 1);
    CHECK(CBLQuery_ColumnName(query, 0) == "name"_sl);

    alloc_slice explanation(CBLQuery_Explain(query));
    cerr << string(explanation);

    static const slice kExpectedFirst[3] = {"Tyesha"_sl,  "Eddie"_sl,     "Diedre"_sl};
    static const slice kExpectedLast [3] = {"Loehrer"_sl, "Colangelo"_sl, "Clinton"_sl};

    int n = 0;
    results = CBLQuery_Execute(query, &error);
    REQUIRE(results);
    while (CBLResultSet_Next(results)) {
        FLValue name = CBLResultSet_ValueAtIndex(results, 0);
        CHECK(CBLResultSet_ValueForKey(results, "name") == name);
        FLDict dict = FLValue_AsDict(name);
        CHECK(dict);
        slice first  = FLValue_AsString(FLDict_Get(dict, "first"_sl));
        slice last   = FLValue_AsString(FLDict_Get(dict, "last"_sl));
        REQUIRE(n < 3);
        CHECK(first == kExpectedFirst[n]);
        CHECK(last == kExpectedLast[n]);
        ++n;
        cerr << first << " " << last << "\n";
    }
    CHECK(n == 3);
}


TEST_CASE_METHOD(QueryTest, "Query Parameters") {
    CBLError error;
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            cerr << "Creating index\n";
            CBLIndexSpec index = {};
            index.type = kCBLValueIndex;
            index.keyExpressionsJSON = R"(["contact.address.zip"])";
            CHECK(CBLDatabase_CreateIndex(db.ref(), "zips", index, &error));
        }

        int errPos;
        query = CBLQuery_New(db.ref(), kCBLN1QLLanguage,
                             "SELECT count(*) AS n WHERE contact.address.zip BETWEEN $zip0 AND $zip1",
                             &errPos, &error);
        REQUIRE(query);

        CHECK(CBLQuery_ColumnCount(query) == 1);
        CHECK(slice(CBLQuery_ColumnName(query, 0)) == "n"_sl);

        alloc_slice explanation(CBLQuery_Explain(query));
        cerr << string(explanation);

        CHECK(CBLQuery_Parameters(query) == nullptr);
        CHECK(CBLQuery_SetParametersAsJSON(query, R"({"zip0":"30000","zip1":"39999"})"));

        FLDict params = CBLQuery_Parameters(query);
        CHECK(FLValue_AsString(FLDict_Get(params, "zip0"_sl)) == "30000"_sl);
        CHECK(FLValue_AsString(FLDict_Get(params, "zip1"_sl)) == "39999"_sl);

        results = CBLQuery_Execute(query, &error);
        REQUIRE(results);
        REQUIRE(CBLResultSet_Next(results));
        auto count = FLValue_AsInt( CBLResultSet_ValueAtIndex(results, 0) );
        CHECK(count == 7);
        CHECK(!CBLResultSet_Next(results));

        CBLQuery_Release(query);
        query = nullptr;
        CBLResultSet_Release(results);
        results = nullptr;
    }
}


static int countResults(CBLResultSet *results) {
    int n = 0;
    while (CBLResultSet_Next(results))
        ++n;
    return n;
}


TEST_CASE_METHOD(QueryTest, "Query Listener") {
    CBLError error;
    query = CBLQuery_New(db.ref(), kCBLN1QLLanguage,
                         "SELECT name WHERE birthday like '1959-%' ORDER BY birthday",
                         nullptr, &error);
    REQUIRE(query);

    CBLResultSet *results = CBLQuery_Execute(query, &error);
    CHECK(countResults(results) == 3);
    CBLResultSet_Release(results);

    cerr << "Adding listener\n";
    listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query) {
        auto self = (QueryTest*)context;
        CBLError error;
        auto newResults = CBLQuery_CurrentResults(query, self->listenerToken, &error);
        CHECK(newResults);
        self->resultCount = countResults(newResults);
    }, this);

    cerr << "Waiting for listener...\n";
    resultCount = -1;
    while (resultCount < 0)
        this_thread::sleep_for(chrono::milliseconds(100));
    CHECK(resultCount == 3);
    resultCount = -1;

    cerr << "Deleting a doc...\n";
    const CBLDocument *doc = CBLDatabase_GetDocument(db.ref(), "0000012");
    REQUIRE(doc);
    CHECK(CBLDocument_Delete(doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLDocument_Release(doc);

    cerr << "Waiting for listener again...\n";
    while (resultCount < 0)
        this_thread::sleep_for(chrono::milliseconds(100));
    CHECK(resultCount == 2);
}
