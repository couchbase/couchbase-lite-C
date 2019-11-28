//
// CBLTest.cc
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

#define CATCH_CONFIG_CONSOLE_WIDTH 120

#include "CBLTest.hh"
#include "fleece/slice.hh"
#include <fstream>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace std;
using namespace fleece;


static string databaseDir() {
#ifndef WIN32
    string dir = "/tmp/CBL_C_tests";
    if (mkdir(dir.c_str(), 0744) != 0 && errno != EEXIST)
        FAIL("Can't create temp directory: errno " << errno);
#else
    string dir = "C:\\tmp\\CBL_C_tests";
    if (_mkdir(dir.c_str()) != 0 && errno != EEXIST)
        FAIL("Can't create temp directory: errno " << errno);
#endif
    return dir;
}


std::string CBLTest::kDatabaseDir = databaseDir();
const char* const CBLTest::kDatabaseName = "CBLtest";

const CBLDatabaseConfiguration CBLTest::kDatabaseConfiguration = []{
    CBLDatabaseConfiguration config = {kDatabaseDir.c_str()};
    config.flags = kCBLDatabase_Create;
    return config;
}();


CBLTest::CBLTest() {
    CBLError error;
    if (!CBL_DeleteDatabase(kDatabaseName, kDatabaseConfiguration.directory, &error) && error.code != 0)
        FAIL("Can't delete temp database: " << error.domain << "/" << error.code);
    db = CBLDatabase_Open(kDatabaseName, &kDatabaseConfiguration, &error);
    REQUIRE(db);
}


CBLTest::~CBLTest() {
    if (db) {
        CBLError error;
        if (!CBLDatabase_Close(db, &error))
            WARN("Failed to close database: " << error.domain << "/" << error.code);
        CBLDatabase_Release(db);
    }
    if (CBL_InstanceCount() > 0)
        CBL_DumpInstances();
    CHECK(CBL_InstanceCount() == 0);
}


#pragma mark - C++ TEST CLASS:


std::string& CBLTest_Cpp::kDatabaseDir = CBLTest::kDatabaseDir;
const char* const & CBLTest_Cpp::kDatabaseName = CBLTest::kDatabaseName;


CBLTest_Cpp::CBLTest_Cpp()
:db(openEmptyDatabaseNamed(kDatabaseName))
{ }


CBLTest_Cpp::~CBLTest_Cpp() {
    db.close();
    db = nullptr;

    if (CBL_InstanceCount() > 0) {
        WARN("*** LEAKED OBJECTS: ***");
        CBL_DumpInstances();
    }
    CHECK(CBL_InstanceCount() == 0);
}


cbl::Database CBLTest_Cpp::openEmptyDatabaseNamed(const char *name) {
    cbl::Database::deleteDatabase(name, CBLTest::kDatabaseConfiguration.directory);
    cbl::Database emptyDB = cbl::Database(name, CBLTest::kDatabaseConfiguration);
    REQUIRE(emptyDB);
    return emptyDB;
}


string GetTestFilePath(const std::string &filename) {
    static string sTestFilesPath;
    if (sTestFilesPath.empty()) {
#ifdef __APPLE__
        auto bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.couchbase.CouchbaseLiteTests"));
        if (bundle) {
            auto url = CFBundleCopyResourcesDirectoryURL(bundle);
            CFAutorelease(url);
            url = CFURLCopyAbsoluteURL(url);
            CFAutorelease(url);
            auto path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
            CFAutorelease(path);
            char pathBuf[1000];
            CFStringGetCString(path, pathBuf, sizeof(pathBuf), kCFStringEncodingUTF8);
            strlcat(pathBuf, "/", sizeof(pathBuf));
            sTestFilesPath = pathBuf;
        } else
#endif
        {
#ifdef WIN32
            sTestFilesPath = "..\\test\\";
#else
            sTestFilesPath = "test/";
#endif
        }
    }
    return sTestFilesPath + filename;
}


bool ReadFileByLines(const string &path, const function<bool(FLSlice)> &callback) {
    INFO("Reading lines from " << path);
    fstream fd(path.c_str(), ios_base::in);
    REQUIRE(fd);
    vector<char> buf(1000000); // The Wikipedia dumps have verrry long lines
    while (fd.good()) {
        fd.getline(buf.data(), buf.size());
        auto len = fd.gcount();
        if (len <= 0)
            break;
        REQUIRE(buf[len-1] == '\0');
        --len;
        if (!callback({buf.data(), (size_t)len}))
            return false;
    }
    REQUIRE(fd.eof());
    return true;
}



// Read a file that contains a JSON document per line. Every line becomes a document.
unsigned ImportJSONLines(string&& path, CBLDatabase* database) {
    CBL_Log(kCBLLogDomainDatabase, CBLLogInfo, "Reading %s ...  ", path.c_str());
    CBLError error;
    unsigned numDocs = 0;

    CHECK(CBLDatabase_BeginBatch(database, &error));
    ReadFileByLines(path, [&](FLSlice line) {
        char docID[20];
        sprintf(docID, "%07u", numDocs+1);
        auto doc = CBLDocument_New(docID);
        REQUIRE(CBLDocument_SetPropertiesAsJSON(doc, slice(line).asString().c_str(), &error));
        auto savedDoc = CBLDatabase_SaveDocument(database, doc, kCBLConcurrencyControlFailOnConflict, &error);
        CHECK(savedDoc);
        CBLDocument_Release(doc);
        CBLDocument_Release(savedDoc);
        ++numDocs;
        return true;
    });
    CBL_Log(kCBLLogDomainDatabase, CBLLogInfo, "Committing %u docs...", numDocs);
    CHECK(CBLDatabase_EndBatch(database, &error));
    return numDocs;
}
