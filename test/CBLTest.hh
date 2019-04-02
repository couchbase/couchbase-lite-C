//
// CBLTest.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "CouchbaseLite.h"
#include "CouchbaseLite.hh"
#include "catch.hpp"
#include <string>


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


    cbl::Database db;
};
