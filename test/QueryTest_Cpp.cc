//
// QueryTest_Cpp.cc
//
// Copyright © 2022 Couchbase. All rights reserved.
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
#include "CBLTest_Cpp.hh"
#include "cbl/CouchbaseLite.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <iostream>
#include <mutex>
#include <thread>
#include <atomic>

using namespace std;
using namespace fleece;
using namespace cbl;

class QueryTest_Cpp : public CBLTest_Cpp {
public:
    QueryTest_Cpp() {
        ImportJSONLines(GetTestFilePath("names_100.json"), db.ref());
    }
};


TEST_CASE_METHOD(QueryTest_Cpp, "Query C++ API", "[Query][QueryCpp]") {
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

TEST_CASE_METHOD(QueryTest_Cpp, "Query Listener C++ API", "[Query][QueryCpp]") {
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
            CHECK(change.query() == query);
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

TEST_CASE_METHOD(QueryTest_Cpp, "Empty Query Listener C++", "[Query][QueryCpp]") {
    Query::ChangeListener listenerToken;
    CHECK(!listenerToken.context());
    CHECK(!listenerToken.token());
    
    bool threw = false;
    try {
        ExpectingExceptions x;
        listenerToken.results();
    } catch(runtime_error &x) {
        threw = true;
    }
    CHECK(threw);
    
    listenerToken.remove(); // Noops
}

TEST_CASE_METHOD(QueryTest_Cpp, "Query Listener C++ Move Operation", "[Query][QueryCpp]") {
    Query query(db, kCBLN1QLLanguage, "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday");
    
    std::atomic_int resultCount{-1};
    
    Query::ChangeListener listenerToken;
    
    // Move assignment:
    listenerToken = query.addChangeListener([&](Query::Change change) {
        ResultSet rs = change.results();
        resultCount = countResults(rs);
        CHECK(change.query() == query);
    });
    
    CHECK(listenerToken.context());
    CHECK(listenerToken.token());
    
    // Waiting for the first called:
    while (resultCount < 0)
        this_thread::sleep_for(100ms);
    CHECK(resultCount == 3);
    resultCount = -1;
    
    // Move constructor:
    Query::ChangeListener listenerToken2 = move(listenerToken);
    CHECK(listenerToken2.context());
    CHECK(listenerToken2.token());
    
#ifndef __clang_analyzer__ // Exclude the code from being compiled for analyzer
    CHECK(!listenerToken.context());
    CHECK(!listenerToken.token());
    listenerToken.remove(); // Noops
#endif

    cerr << "Deleting a doc...\n";
    Document doc = db.getDocument("0000012");
    REQUIRE(doc);
    REQUIRE(db.deleteDocument(doc, kCBLConcurrencyControlLastWriteWins));

    cerr << "Waiting for listener again...\n";
    while (resultCount < 0)
        this_thread::sleep_for(100ms);
    CHECK(resultCount == 2);
    
    listenerToken2.remove();
    CHECK(!listenerToken2.context());
    CHECK(!listenerToken2.token());
    
    // https://issues.couchbase.com/browse/CBL-2147
    // Add a small sleep to ensure async cleanup in LiteCore's LiveQuerier's _stop() when the
    // listenerToken is destructed is done bfore before checking instance leaking in
    // CBLTest_Cpp's destructor:
    cerr << "Sleeping to ensure async cleanup ..." << endl;
    this_thread::sleep_for(500ms);
}

TEST_CASE_METHOD(QueryTest_Cpp, "C++ Query Parameters", "[Query][QueryCpp]") {
    Query query = Query(db, kCBLN1QLLanguage, "SELECT count(*) AS n FROM _ WHERE contact.address.zip BETWEEN $zip0 AND $zip1");
    CHECK(!query.parameters());
    
    auto params = MutableDict::newDict();
    params["zip0"] = "30000";
    params["zip1"] = "39999";
    query.setParameters(params);
    
    auto readParams = query.parameters();
    CHECK(readParams["zip0"].asString() == "30000");
    CHECK(readParams["zip1"].asString() == "39999");
    
    auto results = query.execute();
    int n = 0;
    for (auto &result : results) {
        CHECK(result.count() == 1);
        CHECK(result[0].asInt() == 7);
        ++n;
    }
    CHECK(n == 1);
}
