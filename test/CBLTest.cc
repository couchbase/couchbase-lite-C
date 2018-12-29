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


std::string CBLTest::kDatabaseDir;
const char* const CBLTest::kDatabaseName = "cbl_test";


CBLTest::CBLTest() {
    if (kDatabaseDir.empty()) {
        kDatabaseDir = "/tmp/CBL_C_tests";  // TODO // COMPAT
        if (mkdir(kDatabaseDir.c_str(), 0744) != 0 && errno != EEXIST)
            FAIL("Can't create temp directory: errno " << errno);
    }
    CBLDatabaseConfiguration config = {kDatabaseDir.c_str()};
    CBLError error;
    if (!cbl_deleteDB(kDatabaseName, config.directory, &error) && error.code != 0)
        FAIL("Can't delete temp database: " << error.domain << "/" << error.code);
    db = cbl_db_open(kDatabaseName, &config, &error);
    REQUIRE(db);
}


CBLTest::~CBLTest() {
    if (db) {
        CBLError error;
        if (!cbl_db_close(db, &error))
            WARN("Failed to close database: " << error.domain << "/" << error.code);
        cbl_db_release(db);
    }
}


#include "LibC++Debug.cc"
#include "Backtrace.cc"
