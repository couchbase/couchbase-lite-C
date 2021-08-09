//
// DatabaseTest_Android.cc
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

#ifndef _MSC_VER
#include <sys/stat.h>
#endif

using namespace std;
using namespace fleece;

static string databaseDir() {
#ifdef __APPLE__
    string dir = "/tmp/CBL_C_Android_tests";
    if (mkdir(dir.c_str(), 0744) != 0 && errno != EEXIST)
        FAIL("Can't create temp directory: errno ");
#else
    string dir = "C:\\tmp\\CBL_C_Android_tests";
    if (_mkdir(dir.c_str()) != 0 && errno != EEXIST)
        FAIL("Can't create temp directory: errno " << errno);
#endif
    return dir;
}

static string tempDir() {
#ifdef __APPLE__
    string dir = "/tmp/CBL_C_Android_tests_temp";
    if (mkdir(dir.c_str(), 0744) != 0 && errno != EEXIST)
        FAIL("Can't create temp directory: errno ");
#else
    string dir = "C:\\tmp\\CBL_C_Android_tests_temp";
    if (_mkdir(dir.c_str()) != 0 && errno != EEXIST)
        FAIL("Can't create temp directory: errno " << errno);
#endif
    return dir;
}

/**
 Tests for testing \ref CBL_init function for initializing context information
 for Android. This test is not included in build until we could run developer tests on android.
 
 There are steps to do to run test this in XCode.
 1. Rename __ANDROID__ in ifdef or ifndef statement in any part of the code in include and
    src folder to __CBLTEST_ANDROID__ and add #define __CBLTEST_ANDROID__ 1 in CBLPlatform.h.
    (Note: Define __ANDROID__ will not work as it will interfere C++ lib)
 2. Add CBLDatabase+Android.cc and CBLPlatform_CAPI+Android.cc to CBL_C target.
 3. Add DatabaseTEST_Android.cc to CBL_Tests and CouchbaseLiteTests target.
 4. Add CBL_Init symbole to CBL_Exports.txt
 5. Run the test.
 */
class AndroidTest {
public:
    static const slice kDatabaseName;
    string dbDir;
    string tmpDir;
    
    AndroidTest() {
        dbDir = databaseDir();
        tmpDir = tempDir();
        
        CBLError error;
        if (!CBL_DeleteDatabase(kDatabaseName, FLStr(dbDir.c_str()), &error) && error.code != 0)
            FAIL("Can't delete test database: " << error.domain << "/" << error.code);
    }
    
    CBLDatabase *db {nullptr};
};

slice const AndroidTest::kDatabaseName = "CBLAndroidTest";


TEST_CASE_METHOD(AndroidTest, "Not init context", "[Android]") {
    ExpectingExceptions x;
    auto config = CBLDatabaseConfiguration_Default();
    config.directory = FLStr(dbDir.c_str());
    
    CBLError error;
    CBLDatabase *db = CBLDatabase_Open(kDatabaseName, &config, &error);
    CHECK(!db);
    CHECK(error.code == kCBLErrorUnsupported);
}

TEST_CASE_METHOD(AndroidTest, "Invalid context", "[Android]") {
    CBLInitContext context = {};
    
    ExpectingExceptions x;
    CBLError error;
    CHECK(!CBL_Init(&context, &error));
    CHECK(error.code == kCBLErrorInvalidParameter);
}

TEST_CASE_METHOD(AndroidTest, "Context File Directory Not Exists", "[Android]") {
    CBLInitContext context = {};
    context.filesDir = "/tmp/CBL_C_tests_Not_Exists";
    context.tempDir = "/tmp/CBL_C_tests_Not_Exists";
    
    ExpectingExceptions x;
    CBLError error;
    CHECK(!CBL_Init(&context, &error));
    CHECK(error.code == kCBLErrorNotFound);
}

TEST_CASE_METHOD(AndroidTest, "Context Temp Directory Not Exists", "[Android]") {
    CBLInitContext context = {};
    context.filesDir = "/tmp/CBL_C_tests";
    context.tempDir = "/tmp/CBL_C_tests_Not_Exists";
    
    ExpectingExceptions x;
    CBLError error;
    CHECK(!CBL_Init(&context, &error));
    CHECK(error.code == kCBLErrorNotFound);
}

TEST_CASE_METHOD(AndroidTest, "Valid Context", "[Android]") {
    CBLError error;
    CBLInitContext context = {};
    context.filesDir = dbDir.c_str();
    context.tempDir = tmpDir.c_str();
    CHECK(CBL_Init(&context, &error));
    
    auto config = CBLDatabaseConfiguration_Default();
    string defaultDir = dbDir + kPathSeparator + "CouchbaseLite";
    CHECK(config.directory == FLStr(defaultDir.c_str()));
    
    auto db = CBLDatabase_Open(kDatabaseName, &config, &error);
    CHECK(db);
    CHECK(CBLDatabase_Delete(db, &error));
    
    db = CBLDatabase_Open(kDatabaseName, nullptr, &error);
    CHECK(db);
    CHECK(CBLDatabase_Delete(db, &error));
}
