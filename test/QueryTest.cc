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
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <iostream>
#include <thread>
#include <atomic>

using namespace std;
using namespace fleece;


class QueryTest : public CBLTest {
public:
    QueryTest() {
        ImportJSONLines(GetTestFilePath("names_100.json"), db);
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


TEST_CASE_METHOD(QueryTest, "Invalid Query", "[Query][!throws]") {
    CBLError error;
    int errPos;
    {
        ExpectingExceptions x;
        CBL_Log(kCBLLogDomainQuery, CBLLogWarning, "INTENTIONALLY THROWING EXCEPTION!");
        query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                        "SELECT name WHERE"_sl,
                                        &errPos, &error);
    }
    REQUIRE(!query);
    CHECK(errPos == 17);
    CHECK(error.domain == CBLDomain);
    CHECK(error.code == CBLErrorInvalidQuery);
}


TEST_CASE_METHOD(QueryTest, "Query", "[Query]") {
    CBLError error;
    int errPos;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    &errPos, &error);
    REQUIRE(query);

    CHECK(CBLQuery_ColumnCount(query) == 1);
    CHECK(CBLQuery_ColumnName(query, 0) == "name"_sl);

    alloc_slice explanation(CBLQuery_Explain(query));
    cerr << string(explanation);

    static const slice kExpectedFirst[3] = {"Tyesha",  "Eddie",     "Diedre"};
    static const slice kExpectedLast [3] = {"Loehrer", "Colangelo", "Clinton"};

    int n = 0;
    results = CBLQuery_Execute(query, &error);
    REQUIRE(results);
    CHECK(CBLResultSet_GetQuery(results) == query);
    
    while (CBLResultSet_Next(results)) {
        FLValue name = CBLResultSet_ValueAtIndex(results, 0);
        CHECK(CBLResultSet_ValueForKey(results, "name"_sl) == name);
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


TEST_CASE_METHOD(QueryTest, "Query Parameters", "[Query]") {
    CBLError error;
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            cerr << "Creating index\n";
            CBLValueIndexConfiguration config = {};
            config.expressionLanguage = kCBLJSONLanguage;
            config.expressions = R"(["contact.address.zip"])"_sl;
            CHECK(CBLDatabase_CreateValueIndex(db, "zips"_sl, config, &error));
        }

        int errPos;
        query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                        "SELECT count(*) AS n WHERE contact.address.zip BETWEEN $zip0 AND $zip1"_sl,
                                        &errPos, &error);
        REQUIRE(query);

        CHECK(CBLQuery_ColumnCount(query) == 1);
        CHECK(CBLQuery_ColumnName(query, 0) == "n"_sl);

        alloc_slice explanation(CBLQuery_Explain(query));
        cerr << string(explanation);

        CHECK(CBLQuery_Parameters(query) == nullptr);
        {
            auto params = MutableDict::newDict();
            params["zip0"] = "30000";
            params["zip1"] = "39999";
            CBLQuery_SetParameters(query, params);
        }

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


TEST_CASE_METHOD(QueryTest, "Create and Delete Value Index", "[Query]") {
    CBLError error;
    int errPos;
    
    CBLValueIndexConfiguration index1 = {};
    index1.expressionLanguage = kCBLN1QLLanguage;
    index1.expressions = "name.first"_sl;
    CHECK(CBLDatabase_CreateValueIndex(db, "index1"_sl, index1, &error));
    
    CBLValueIndexConfiguration index2 = {};
    index2.expressionLanguage = kCBLJSONLanguage;
    index2.expressions = R"([[".name.last"]])"_sl;
    CHECK(CBLDatabase_CreateValueIndex(db, "index2"_sl, index2, &error));
    
    FLArray indexNames = CBLDatabase_GetIndexNames(db);
    CHECK(FLArray_Count(indexNames) == 2);
    CHECK(Array(indexNames).toJSONString() == R"(["index1","index2"])");
    
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name.first FROM _ ORDER BY name.first"_sl,
                                    &errPos, &error);
    
    alloc_slice explanation1(CBLQuery_Explain(query));
    CHECK(explanation1.find("SCAN TABLE kv_default AS _ USING INDEX index1"_sl));
    CBLQuery_Release(query);
    
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name.last FROM _ ORDER BY name.last"_sl,
                                    &errPos, &error);
    
    alloc_slice explanation2(CBLQuery_Explain(query));
    CHECK(explanation2.find("SCAN TABLE kv_default AS _ USING INDEX index2"_sl));
    CBLQuery_Release(query);
    query = nullptr;
    
    CHECK(CBLDatabase_DeleteIndex(db, "index1"_sl, &error));
    CHECK(CBLDatabase_DeleteIndex(db, "index2"_sl, &error));
    
    indexNames = CBLDatabase_GetIndexNames(db);
    CHECK(FLArray_Count(indexNames) == 0);
    CHECK(Array(indexNames).toJSONString() == R"([])");
}


TEST_CASE_METHOD(QueryTest, "Create and Delete Full-Text Index", "[Query]") {
    CBLError error;
    int errPos;

    CBLFullTextIndexConfiguration index1 = {};
    index1.expressionLanguage = kCBLN1QLLanguage;
    index1.expressions = "product.description"_sl;
    index1.ignoreAccents = true;
    CHECK(CBLDatabase_CreateFullTextIndex(db, "index1"_sl, index1, &error));
    
    CBLFullTextIndexConfiguration index2 = {};
    index2.expressionLanguage = kCBLJSONLanguage;
    index2.expressions = R"([[".product.summary"]])"_sl;
    index2.ignoreAccents = false;
    index2.language = "en/english"_sl;
    CHECK(CBLDatabase_CreateFullTextIndex(db, "index2"_sl, index2, &error));
    
    FLArray indexNames = CBLDatabase_GetIndexNames(db);
    CHECK(FLArray_Count(indexNames) == 2);
    CHECK(Array(indexNames).toJSONString() == R"(["index1","index2"])");
    
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT product.name FROM _ WHERE match('index1', 'avocado')"_sl,
                                    &errPos, &error);
    
    alloc_slice explanation1(CBLQuery_Explain(query));
    CHECK(explanation1.find("SCAN TABLE kv_default::index1 AS fts1"_sl));
    CBLQuery_Release(query);
    
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT product.name FROM _ WHERE match('index2', 'chilli')"_sl,
                                    &errPos, &error);
    
    alloc_slice explanation2(CBLQuery_Explain(query));
    CHECK(explanation2.find("SCAN TABLE kv_default::index2 AS fts1"_sl));
    CBLQuery_Release(query);
    query = nullptr;
    
    CHECK(CBLDatabase_DeleteIndex(db, "index1"_sl, &error));
    CHECK(CBLDatabase_DeleteIndex(db, "index2"_sl, &error));
    
    indexNames = CBLDatabase_GetIndexNames(db);
    CHECK(FLArray_Count(indexNames) == 0);
    CHECK(Array(indexNames).toJSONString() == R"([])");
}


TEST_CASE_METHOD(QueryTest, "Query Result As Dict", "[Query]") {
    CBLError error;
    int errPos;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    &errPos, &error);
    REQUIRE(query);

    CHECK(CBLQuery_ColumnCount(query) == 1);
    CHECK(CBLQuery_ColumnName(query, 0) == "name"_sl);

    alloc_slice explanation(CBLQuery_Explain(query));
    cerr << string(explanation);

    static const slice kExpectedFirst[3] = {"Tyesha",  "Eddie",     "Diedre"};
    static const slice kExpectedLast [3] = {"Loehrer", "Colangelo", "Clinton"};

    int n = 0;
    results = CBLQuery_Execute(query, &error);
    REQUIRE(results);
    while (CBLResultSet_Next(results)) {
        FLDict result = CBLResultSet_ResultDict(results);
        FLValue name = FLDict_Get(result, "name"_sl);
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


TEST_CASE_METHOD(QueryTest, "Query Result As Array", "[Query]") {
    CBLError error;
    int errPos;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    &errPos, &error);
    REQUIRE(query);

    CHECK(CBLQuery_ColumnCount(query) == 1);
    CHECK(CBLQuery_ColumnName(query, 0) == "name"_sl);

    alloc_slice explanation(CBLQuery_Explain(query));
    cerr << string(explanation);

    static const slice kExpectedFirst[3] = {"Tyesha",  "Eddie",     "Diedre"};
    static const slice kExpectedLast [3] = {"Loehrer", "Colangelo", "Clinton"};

    int n = 0;
    results = CBLQuery_Execute(query, &error);
    REQUIRE(results);
    while (CBLResultSet_Next(results)) {
        FLArray result = CBLResultSet_ResultArray(results);
        REQUIRE(FLArray_Count(result) == 1);
        FLValue name = FLArray_Get(result, 0);
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


static int countResults(CBLResultSet *results) {
     int n = 0;
     while (CBLResultSet_Next(results))
         ++n;
     return n;
}
 
TEST_CASE_METHOD(QueryTest, "Query Listener", "[Query]") {
    CBLError error;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    nullptr, &error);
    REQUIRE(query);

    CBLResultSet *results = CBLQuery_Execute(query, &error);
    CHECK(countResults(results) == 3);
    CBLResultSet_Release(results);

    resultCount = -1;
    
    cerr << "Adding listener\n";
    listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        auto self = (QueryTest*)context;
        CBLError error;
        auto newResults = CBLQuery_CopyCurrentResults(query, token, &error);
        CHECK(newResults);
        self->resultCount = countResults(newResults);
        CBLResultSet_Release(newResults);
    }, this);

    cerr << "Waiting for listener...\n";
    while (resultCount < 0)
        this_thread::sleep_for(100ms);
    CHECK(resultCount == 3);
    resultCount = -1;

    cerr << "Deleting a doc...\n";
    const CBLDocument *doc = CBLDatabase_GetDocument(db, "0000012"_sl, &error);
    REQUIRE(doc);
    CHECK(CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLDocument_Release(doc);

    cerr << "Waiting for listener again...\n";
    while (resultCount < 0)
        this_thread::sleep_for(100ms);
    CHECK(resultCount == 2);
    
    // https://issues.couchbase.com/browse/CBL-2117
    // Remove listener token and sleep to ensure async cleanup in LiteCore's LiveQuerier's _stop()
    // functions is done before checking instance leaking in CBLTest's destructor:
    CBLListener_Remove(listenerToken);
    listenerToken = nullptr;
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}


#pragma mark - C++ API:


#include "CBLTest_Cpp.hh"

using namespace cbl;


class QueryTest_Cpp : public CBLTest_Cpp {
public:
    QueryTest_Cpp() {
        ImportJSONLines(GetTestFilePath("names_100.json"), db.ref());
    }
};


TEST_CASE_METHOD(QueryTest_Cpp, "Query C++ API", "[Query]") {
    Query query(db, kCBLN1QLLanguage, "SELECT name WHERE birthday like '1959-%' ORDER BY birthday");

    CHECK(query.columnNames() == vector<string>({"name"}));

    alloc_slice explanation(query.explain());
    cerr << string(explanation);

    static const slice kExpectedFirst[3] = {"Tyesha",  "Eddie",     "Diedre"};
    static const slice kExpectedLast [3] = {"Loehrer", "Colangelo", "Clinton"};

    int n = 0;
    auto results = query.execute();
    for (auto &result : results) {
        Value name = result[0];
        CHECK(result["name"] == name);
        Dict dict = name.asDict();
        CHECK(dict);
        slice first = dict["first"].asString();
        slice last  = dict["last"].asString();
        REQUIRE(n < 3);
        cerr << "'" << first << "', '" << last << "'\n";
        CHECK(first == kExpectedFirst[n]);
        CHECK(last == kExpectedLast[n]);
        ++n;
        cerr << first << " " << last << "\n";
    }
    CHECK(n == 3);
}

/**
 * CBL-2147 : Disable the test until the issue is fixed:
static int countResults(ResultSet &results) {
    int n = 0;
    for (CBL_UNUSED auto &result : results)
        ++n;
    return n;
}

TEST_CASE_METHOD(QueryTest_Cpp, "Query Listener, C++ API", "[Query]") {
    Query query(db, kCBLN1QLLanguage, "SELECT name WHERE birthday like '1959-%' ORDER BY birthday");
    {
        auto rs = query.execute();
        CHECK(countResults(rs) == 3);
    }

    cerr << "Adding listener\n";
    std::atomic_int resultCount{-1};
    Query::ChangeListener listenerToken = query.addChangeListener([&](Query::Change change) {
        ResultSet rs = change.results();
        resultCount = countResults(rs);
    });

    cerr << "Waiting for listener...\n";
    while (resultCount < 0)
        this_thread::sleep_for(100ms);
    CHECK(resultCount == 3);
    resultCount = -1;

    cerr << "Deleting a doc...\n";
    Document doc = db.getDocument("0000012");
    REQUIRE(doc);
    REQUIRE(db.deleteDocument(doc, kCBLConcurrencyControlLastWriteWins));

    cerr << "Waiting for listener again...\n";
    while (resultCount < 0)
        this_thread::sleep_for(100ms);
    CHECK(resultCount == 2);
}
*/
