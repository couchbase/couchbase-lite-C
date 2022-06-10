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
#include <mutex>
#include <thread>
#include <atomic>

using namespace std;
using namespace fleece;

CBL_START_WARNINGS_SUPPRESSION
CBL_IGNORE_DEPRECATED_API

class QueryTest : public CBLTest {
public:
    QueryTest() {
        ImportJSONLines(GetTestFilePath("names_100.json"), db);
    }

    ~QueryTest() {
        CBLResultSet_Release(results);
        CBLQuery_Release(query);
    }

    CBLQuery *query =nullptr;
    CBLResultSet *results =nullptr;
};


static int countResults(CBLResultSet *results) {
     int n = 0;
     while (CBLResultSet_Next(results))
         ++n;
     return n;
}

/** For keeping track of listener callback result in LiveQuery tests. */
struct ListenerState {
    int count() {
        lock_guard<mutex> lock(_mutex);
        return _count;
    }
    
    int resultCount() {
        lock_guard<mutex> lock(_mutex);
        return _resultCount;
    }
    
    void reset() {
        lock_guard<mutex> lock(_mutex);
        _count = 0;
        _resultCount = -1;
    }
    
    void receivedCallback(void *context, CBLQuery* query, CBLListenerToken* token) {
        lock_guard<mutex> lock(_mutex);
        ++_count;
        
        CBLError error;
        auto newResults = CBLQuery_CopyCurrentResults(query, token, &error);
        REQUIRE(newResults);
        _resultCount = countResults(newResults);
        CBLResultSet_Release(newResults);
    }
    
    bool waitForCount(int target) {
        int timeoutCount = 0;
        while (timeoutCount++ < 50) {
            {
                lock_guard<mutex> lock(_mutex);
                if (_count == target)
                    return true;
            }
            this_thread::sleep_for(100ms);
        }
        return false;
    }
    
private:
    std::mutex _mutex;
    int _count {0};
    int _resultCount {-1};
};


TEST_CASE_METHOD(QueryTest, "Invalid Query", "[Query][!throws]") {
    CBLError error;
    int errPos;
    {
        ExpectingExceptions x;
        CBL_Log(kCBLLogDomainQuery, kCBLLogWarning, "INTENTIONALLY THROWING EXCEPTION!");
        query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                        "SELECT name WHERE"_sl,
                                        &errPos, &error);
    }
    REQUIRE(!query);
    CHECK(errPos == 17);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidQuery);
}


TEST_CASE_METHOD(QueryTest, "Query", "[Query]") {
    CBLError error;
    int errPos;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
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
                                        "SELECT count(*) AS n FROM _ WHERE contact.address.zip BETWEEN $zip0 AND $zip1"_sl,
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
                                    "SELECT product.name FROM _ WHERE match(index1, 'avocado')"_sl,
                                    &errPos, &error);
    
    alloc_slice explanation1(CBLQuery_Explain(query));
    CHECK(explanation1.find("SCAN TABLE kv_default::index1 AS fts1"_sl));
    CBLQuery_Release(query);
    
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT product.name FROM _ WHERE match(index2, 'chilli')"_sl,
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
                                    "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
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
                                    "SELECT name, foo FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    &errPos, &error);
    REQUIRE(query);

    REQUIRE(CBLQuery_ColumnCount(query) == 2);
    CHECK(CBLQuery_ColumnName(query, 0) == "name"_sl);
    CHECK(CBLQuery_ColumnName(query, 1) == "foo"_sl);

    alloc_slice explanation(CBLQuery_Explain(query));
    cerr << string(explanation);

    static const slice kExpectedFirst[3] = {"Tyesha",  "Eddie",     "Diedre"};
    static const slice kExpectedLast [3] = {"Loehrer", "Colangelo", "Clinton"};

    int n = 0;
    results = CBLQuery_Execute(query, &error);
    REQUIRE(results);
    while (CBLResultSet_Next(results)) {
        FLArray result = CBLResultSet_ResultArray(results);
        REQUIRE(FLArray_Count(result) == 2);
        FLValue name = FLArray_Get(result, 0);
        FLDict dict = FLValue_AsDict(name);
        CHECK(dict);
        slice first  = FLValue_AsString(FLDict_Get(dict, "first"_sl));
        slice last   = FLValue_AsString(FLDict_Get(dict, "last"_sl));
        REQUIRE(n < 3);
        CHECK(first == kExpectedFirst[n]);
        CHECK(last == kExpectedLast[n]);
        cerr << first << " " << last << "\n";
        FLValue foo = FLArray_Get(result, 1);
        CHECK(FLValue_GetType(foo) == kFLUndefined);
        ++n;
    }
    CHECK(n == 3);
}


TEST_CASE_METHOD(QueryTest, "Query Listener", "[Query][LiveQuery]") {
    CBLError error;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    nullptr, &error);
    REQUIRE(query);

    CBLResultSet *results = CBLQuery_Execute(query, &error);
    CHECK(countResults(results) == 3);
    CBLResultSet_Release(results);
    
    cerr << "Adding listener\n";
    ListenerState state;
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        ((ListenerState*)context)->receivedCallback(context, query, token);
    }, &state);

    cerr << "Waiting for listener...\n";
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 3);
    
    cerr << "Deleting a doc...\n";
    state.reset();
    
    const CBLDocument *doc = CBLDatabase_GetDocument(db, "0000012"_sl, &error);
    REQUIRE(doc);
    CHECK(CBLDatabase_DeleteDocument(db, doc, &error));
    CBLDocument_Release(doc);

    cerr << "Waiting for listener again...\n";
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 2);
    
    // https://issues.couchbase.com/browse/CBL-2117
    // Remove listener token and sleep to ensure async cleanup in LiteCore's LiveQuerier's _stop()
    // functions is done before checking instance leaking in CBLTest's destructor:
    CBLListener_Remove(listenerToken);
    listenerToken = nullptr;
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}


TEST_CASE_METHOD(QueryTest, "Remove Query Listener", "[Query][LiveQuery]") {
    CBLError error;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    nullptr, &error);
    REQUIRE(query);
    
    cerr << "Adding listener\n";
    ListenerState state;
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        ((ListenerState*)context)->receivedCallback(context, query, token);
    }, &state);

    cerr << "Waiting for listener...\n";
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 3);
    
    cerr << "Removing the listener...\n";
    CBLListener_Remove(listenerToken);
    
    cerr << "Deleting a doc...\n";
    const CBLDocument *doc = CBLDatabase_GetDocument(db, "0000012"_sl, &error);
    REQUIRE(doc);
    CHECK(CBLDatabase_DeleteDocument(db, doc, &error));
    CBLDocument_Release(doc);
    
    cerr << "Sleeping to ensure that the listener callback is not called..." << endl;
    this_thread::sleep_for(1000ms); // Max delay before refreshing result in LiteCore is 500ms
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 3);
    
    // Cleanup:
    listenerToken = nullptr;
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}


TEST_CASE_METHOD(QueryTest, "Query Listener and Changing parameters", "[Query][LiveQuery]") {
    CBLError error;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name FROM _ WHERE birthday like $dob ORDER BY birthday"_sl,
                                    nullptr, &error);
    REQUIRE(query);
    
    auto params = MutableDict::newDict();
    params["dob"] = "1959-%";
    CBLQuery_SetParameters(query, params);

    cerr << "Adding listener\n";
    ListenerState state;
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        ((ListenerState*)context)->receivedCallback(context, query, token);
    }, &state);

    cerr << "Waiting for listener...\n";
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 3);
    
    cerr << "Changing parameters\n";
    state.reset();
    params = MutableDict::newDict();
    params["dob"] = "1977-%";
    CBLQuery_SetParameters(query, params);

    cerr << "Waiting for listener again...\n";
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 2);
    
    CBLListener_Remove(listenerToken);
    listenerToken = nullptr;
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}


TEST_CASE_METHOD(QueryTest, "Multiple Query Listeners", "[Query][LiveQuery]") {
    CBLError error;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    nullptr, &error);
    REQUIRE(query);
    
    auto callback = [](void *context, CBLQuery* query, CBLListenerToken* token) {
        ((ListenerState*)context)->receivedCallback(context, query, token);
    };
    
    cerr << "Adding listener\n";
    ListenerState state1;
    auto token1 = CBLQuery_AddChangeListener(query, callback, &state1);
    
    ListenerState state2;
    auto token2 = CBLQuery_AddChangeListener(query, callback, &state2);

    cerr << "Waiting for listener 1...\n";
    state1.waitForCount(1);
    CHECK(state1.resultCount() == 3);
    
    cerr << "Waiting for listener 2...\n";
    state2.waitForCount(1);
    CHECK(state2.resultCount() == 3);

    cerr << "Deleting a doc...\n";
    state1.reset();
    state2.reset();
    
    const CBLDocument *doc = CBLDatabase_GetDocument(db, "0000012"_sl, &error);
    REQUIRE(doc);
    CHECK(CBLDatabase_DeleteDocument(db, doc, &error));
    CBLDocument_Release(doc);

    cerr << "Waiting for listener 1 again...\n";
    state1.waitForCount(1);
    CHECK(state1.resultCount() == 2);
    
    cerr << "Waiting for listener 2 again...\n";
    state2.waitForCount(1);
    CHECK(state2.resultCount() == 2);
    
    cerr << "Adding another listener\n";
    ListenerState state3;
    auto token3 = CBLQuery_AddChangeListener(query, callback, &state3);
    
    cerr << "Waiting for the listener 3...\n";
    state3.waitForCount(1);
    CHECK(state3.resultCount() == 2);
    CHECK(state1.count() == 1);
    CHECK(state2.count() == 1);
    
    CBLListener_Remove(token1);
    CBLListener_Remove(token2);
    CBLListener_Remove(token3);
    
    token1 = nullptr;
    token2 = nullptr;
    token3 = nullptr;
    
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}


TEST_CASE_METHOD(QueryTest, "Query Listener and Coalescing notification", "[Query][LiveQuery]") {
    CBLError error;
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                    nullptr, &error);
    REQUIRE(query);
    
    cerr << "Adding listener\n";
    ListenerState state;
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        ((ListenerState*)context)->receivedCallback(context, query, token);
    }, &state);

    cerr << "Waiting for listener...\n";
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 3);
    
    cerr << "Deleting a doc...\n";
    state.reset();
    REQUIRE(CBLDatabase_DeleteDocumentByID(db, "0000012"_sl, &error));
    REQUIRE(CBLDatabase_DeleteDocumentByID(db, "0000046"_sl, &error));

    cerr << "Sleeping to see if the notification is coalesced ...\n";
    this_thread::sleep_for(1000ms); // Max delay before refreshing result in LiteCore is 500ms
    REQUIRE(state.waitForCount(1));
    CHECK(state.resultCount() == 1);
    
    // https://issues.couchbase.com/browse/CBL-2117
    // Remove listener token and sleep to ensure async cleanup in LiteCore's LiveQuerier's _stop()
    // functions is done before checking instance leaking in CBLTest's destructor:
    CBLListener_Remove(listenerToken);
    listenerToken = nullptr;
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}


#ifdef COUCHBASE_ENTERPRISE

TEST_CASE_METHOD(QueryTest, "Query Encryptable", "[Query]") {
    auto doc = CBLDocument_CreateWithID("doc1"_sl);
    auto props = CBLDocument_MutableProperties(doc);
    
    FLSlot_SetString(FLMutableDict_Set(props, "nosecret"_sl), "No Secret"_sl);
    
    auto secret1 = CBLEncryptable_CreateWithString("Secret 1"_sl);
    FLSlot_SetEncryptableValue(FLMutableDict_Set(props, "secret"_sl), secret1);
    
    auto nestedDict = FLMutableDict_New();
    auto secret2 = CBLEncryptable_CreateWithString("Secret 2"_sl);
    FLSlot_SetEncryptableValue(FLMutableDict_Set(nestedDict, "secret2"_sl), secret2);
    FLSlot_SetDict(FLMutableDict_Set(props, "nested"_sl), nestedDict);
    
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CBLEncryptable_Release(secret1);
    CBLEncryptable_Release(secret2);
    FLMutableDict_Release(nestedDict);
    CBLDocument_Release(doc);
    
    query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                    "SELECT nosecret, secret, secret.value, nested FROM _ WHERE meta().id = 'doc1'"_sl,
                                    nullptr, &error);
    REQUIRE(query);

    results = CBLQuery_Execute(query, &error);
    int n = 0;
    while (CBLResultSet_Next(results)) {
        FLValue value = CBLResultSet_ValueAtIndex(results, 0);
        REQUIRE(value);
        REQUIRE(!FLValue_GetEncryptableValue(value));
        CHECK(FLValue_AsString(value) == "No Secret"_sl);
        
        value = CBLResultSet_ValueAtIndex(results, 1);
        REQUIRE(value);
        auto encValue = FLValue_GetEncryptableValue(value);
        REQUIRE(encValue);
        CHECK(FLValue_AsString(CBLEncryptable_Value(encValue)) == "Secret 1"_sl);
        
        value = CBLResultSet_ValueAtIndex(results, 2);
        REQUIRE(value);
        REQUIRE(!FLValue_GetEncryptableValue(value));
        CHECK(FLValue_AsString(value) == "Secret 1"_sl);
        
        value = FLDict_Get(FLValue_AsDict(CBLResultSet_ValueAtIndex(results, 3)), "secret2"_sl);
        REQUIRE(value);
        encValue = FLValue_GetEncryptableValue(value);
        REQUIRE(encValue);
        CHECK(FLValue_AsString(CBLEncryptable_Value(encValue)) == "Secret 2"_sl);
        
        ++n;
    }
    CHECK(n == 1);
}

#endif


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
    Query query = db.createQuery(kCBLN1QLLanguage, "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday");
    
    CHECK(query.columnNames() == vector<string>({"name"}));

    alloc_slice explanation(query.explain());
    cerr << string(explanation);

    static const slice kExpectedFirst[3] = {"Tyesha",  "Eddie",     "Diedre"};
    static const slice kExpectedLast [3] = {"Loehrer", "Colangelo", "Clinton"};
    static const slice kExpectedJSON [3] = {"{\"name\":{\"first\":\"Tyesha\",\"last\":\"Loehrer\"}}",
                                            "{\"name\":{\"first\":\"Eddie\",\"last\":\"Colangelo\"}}",
                                            "{\"name\":{\"first\":\"Diedre\",\"last\":\"Clinton\"}}"};

    int n = 0;
    auto results = query.execute();
    for (auto &result : results) {
        REQUIRE(result.count() == 1);
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
        CHECK(result.toJSON() == kExpectedJSON[n]);
        
        ++n;
        cerr << first << " " << last << "\n";
    }
    CHECK(n == 3);
}


static int countResults(ResultSet &results) {
    int n = 0;
    for (CBL_UNUSED auto &result : results)
        ++n;
    return n;
}

TEST_CASE_METHOD(QueryTest_Cpp, "Query Listener C++ API", "[Query]") {
    Query query(db, kCBLN1QLLanguage, "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday");
    {
        auto rs = query.execute();
        CHECK(countResults(rs) == 3);
    }

    {
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
    
    // https://issues.couchbase.com/browse/CBL-2147
    // Add a small sleep to ensure async cleanup in LiteCore's LiveQuerier's _stop() when the
    // listenerToken is destructed is done bfore before checking instance leaking in
    // CBLTest_Cpp's destructor:
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}

CBL_STOP_WARNINGS_SUPPRESSION
