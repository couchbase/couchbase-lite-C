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
#include "cbl/CouchbaseLite.h"
#include "CBLPrivate.h"
#include "fleece/slice.hh"
#include <functional>
#include <iostream>
#include <string>

#ifdef WIN32
constexpr char kPathSeparator[] = "\\";
#else
constexpr char kPathSeparator[] = "/";
#endif

#ifdef __APPLE__
#   define CBL_UNUSED __unused
#elif !defined(_MSC_VER)
#   define CBL_UNUSED __attribute__((unused))
#else
#   define CBL_UNUSED
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

static inline std::ostream& operator<< (std::ostream &out, CBLError err) {
    out << "{" << err.domain << ", " << err.code << "}";
    return out;
}

#include "catch.hpp"


class CBLTest {
public:
    static const fleece::alloc_slice kDatabaseDir;
    static const fleece::slice kDatabaseName;
    static const CBLDatabaseConfiguration kDatabaseConfiguration;

    CBLTest();
    ~CBLTest();


    CBLDatabase *db {nullptr};
};


std::string GetTestFilePath(const std::string &filename);

bool ReadFileByLines(const std::string &path, const std::function<bool(FLSlice)> &callback);

unsigned ImportJSONLines(std::string &&path, CBLDatabase* database);


// RAII utility to suppress reporting C++ exceptions (or breaking at them, in the Xcode debugger.)
// Declare an instance when testing something that's expected to throw an exception internally.
struct ExpectingExceptions {
    ExpectingExceptions()   {CBLLog_BeginExpectingExceptions();}
    ~ExpectingExceptions()  {CBLLog_EndExpectingExceptions();}
};
