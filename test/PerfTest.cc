//
// PerfTest.cc
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

#include "CBLTest_Cpp.hh"
#include "Stopwatch.hh"
#include "Benchmark.hh"
#include <fstream>
#include <stdarg.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

using namespace std;
using namespace fleece;

class PerfTest : public CBLTest_Cpp {
public:
    std::string vformat(const char *fmt, va_list args) const __printflike(2, 0) {
        char *cstr = nullptr;
        if (vasprintf(&cstr, fmt, args) < 0)
            throw bad_alloc();
        std::string result(cstr);
        free(cstr);
        return result;
    }
    
    void printLog(const char *fmt, ...) const __printflike(2, 3) {
        va_list args;
        va_start(args, fmt);
        string msg = vformat(fmt, args);
    #ifdef __ANDROID__
        __android_log_write(ANDROID_LOG_INFO, "CBLTests [Perf]", msg.c_str());
    #else
        cout << msg << '\n';
    #endif
        va_end(args);
    }
    
    void printReport(Benchmark& b, const char *what, double scale =1.0, const char *items =nullptr) const {
        auto r = b.range();

        std::string scaleName;
        const char* kTimeScales[] = {"sec", "ms", "us", "ns"};
        double avg = b.average();
        for (unsigned i = 0; i < sizeof(kTimeScales)/sizeof(char*); ++i) {
            if (i > 0)
                scale *= 1000;
            scaleName = kTimeScales[i];
            if (avg*scale >= 1.0)
                break;
        }

        if (items)
            scaleName += std::string("/") + std::string(items);

        printLog("%s: Median %7.3f %s; mean %7.3f; std dev %5.3g; range (%.3f ... %7.3f)",
                 what, b.median()*scale, scaleName.c_str(), b.average()*scale, b.stddev()*scale,
                 r.first*scale, r.second*scale);
    }
    
    void printReport(Stopwatch& st, const char *what, unsigned count, const char *item) const {
        auto time = st.elapsedMS();
        printLog("%s: Took %.3f ms for %u %ss (%.3f ms/%s, or %.0f %ss/sec)",
                what, time, count, item, time/count*1.0, item, count/time*1000, item);
    }
    
    void readRandomDocs(const CBLCollection* collection, size_t numDocs, size_t numDocsToRead) {
        printLog("Reading %zu random docs ...", numDocsToRead);
        Benchmark b;
        constexpr size_t bufSize = 30;
        std::srand(123456);
        for (size_t readNo = 0; readNo < numDocsToRead; ++readNo) {
            char docID[bufSize];
            size_t number = ((unsigned) std::rand() % numDocs) + 1;
            snprintf(docID, bufSize, "%07zu", number);
            b.start();
            CBLError err {};
            auto doc = CBLCollection_GetDocument(collection, FLStr(docID), &err);
            REQUIRE(doc);
            CHECK(CBLDocument_Properties(doc) != nullptr);
            CBLDocument_Release(doc);
            b.stop();
        }
        printReport(b, "Reading Result", 1, "doc");
    }
    
    unsigned query(const char *queryString, bool verbose =false) {
        CBLError error {};
        int errPos;
        auto query = CBLDatabase_CreateQuery(db.ref(), kCBLN1QLLanguage, FLStr(queryString), &errPos, &error);
        REQUIRE(query);
        
        unsigned count = 0;
        auto results = CBLQuery_Execute(query, &error);
        REQUIRE(results);
        while (CBLResultSet_Next(results)) {
            FLDict result = CBLResultSet_ResultDict(results);
            CHECK(result);
            count++;
        }
        CBLResultSet_Release(results);
        CBLQuery_Release(query);
        return count;
    }
};

// NOTE:
// Copy vendor/couchbase-lite-core/C/tests/data/iTunesMusicLibrary.json
// to tests/assets before building and running this test.
TEST_CASE_METHOD(PerfTest, "Benchmark Import iTunesMusicLibrary", "[Perf][.slow]") {
    printLog("Importing docs ...");
    Stopwatch st;
    
    auto coll = db.getDefaultCollection();
    auto numDocs = ImportJSONLines("iTunesMusicLibrary.json", coll.ref());
    st.stop();
    CHECK(numDocs == 12189);
    printReport(st, "Importing Result", numDocs, "doc");
    readRandomDocs(coll.ref(), numDocs, 100000);
}

// NOTE:
// Download https://github.com/arangodb/example-datasets/raw/master/RandomUsers/names_300000.json
// to tests/assets before building and running this test.
TEST_CASE_METHOD(PerfTest, "Benchmark Query Names", "[Perf][.slow]") {
    printLog("Importing docs ...");
    auto coll = db.getDefaultCollection();
    auto numDocs = ImportJSONLines("names_300000.json", coll.ref());
    CHECK(numDocs == 300000);
    
    for (int pass = 0; pass < 2; ++pass) {
        Stopwatch st;
        auto n = query("SELECT meta().id FROM _ WHERE contact.address.state = \"WA\"");
        st.stop();
        CHECK(n == 5053);
        printReport(st, "Query Result", n, "doc");
        
        if (pass == 0) {
            CBLValueIndexConfiguration index = {};
            index.expressionLanguage = kCBLN1QLLanguage;
            index.expressions = FLStr("contact.address.state");
            CBLError error {};
            Stopwatch st2;
            CHECK(CBLCollection_CreateValueIndex(coll.ref(), FLStr("byState"), index, &error));
            st2.stop();
            printReport(st2, "Creating Index Result", 1, "index");
        }
    }
}
