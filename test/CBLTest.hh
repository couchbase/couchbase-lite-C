//
// CBLTest.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

#pragma once
#include "CouchbaseLite.h"
#include "CouchbaseLite.hh"
#include <functional>
#include <iostream>
#include <string>

#ifdef WIN32
constexpr char kPathSeparator[] = "\\";
#else
constexpr char kPathSeparator[] = "/";
#endif

// Has to be declared before including catch.hpp, so Catch macros can use it
namespace fleece {
    static inline std::ostream& operator << (std::ostream &out, slice s) {
        if (s) {
            out << "slice(\"";
            out.write((const char*)s.buf, s.size);
            out << "\")";
        } else {
            out << "nullslice";
        }
        return out;
    }
}

#include "catch.hpp"


class CBLTest {
public:
    static std::string kDatabaseDir;
    static const char* const kDatabaseName;
    static const CBLDatabaseConfiguration kDatabaseConfiguration;

    CBLTest();
    ~CBLTest();


    CBLDatabase *db {nullptr};
};


class CBLTest_Cpp {
public:
    static std::string& kDatabaseDir;
    static const char* const &kDatabaseName;

    CBLTest_Cpp();
    ~CBLTest_Cpp();

    cbl::Database openEmptyDatabaseNamed(const char *name);

    cbl::Database db;
};

std::string GetTestFilePath(const std::string &filename);

bool ReadFileByLines(const std::string &path, const std::function<bool(FLSlice)> &callback);

unsigned ImportJSONLines(std::string &&path, CBLDatabase* database);
