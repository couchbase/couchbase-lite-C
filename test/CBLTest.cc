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
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file

#include "CBLTest.hh"
#include "CaseListReporter.hh"
#include <sys/stat.h>


static std::string databaseDir() {
    std::string dir = "/tmp/CBL_C_tests";  // TODO // COMPAT
    if (mkdir(dir.c_str(), 0744) != 0 && errno != EEXIST)
        FAIL("Can't create temp directory: errno " << errno);
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


CBLTest_Cpp::CBLTest_Cpp() {
    cbl::Database::deleteDatabase(kDatabaseName, kDatabaseDir.c_str());
    db = cbl::Database(kDatabaseName, CBLTest::kDatabaseConfiguration);
    REQUIRE(db);
}


CBLTest_Cpp::~CBLTest_Cpp() {
    db.close();
    db = nullptr;

    if (CBL_InstanceCount() > 0)
        CBL_DumpInstances();
    CHECK(CBL_InstanceCount() == 0);
}


#include "LibC++Debug.cc"
#include "Backtrace.cc"
