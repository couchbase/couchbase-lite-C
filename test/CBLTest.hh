//
// CBLTest.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "CouchbaseLite.h"
#include "catch.hpp"
#include <string>


class CBLTest {
public:
    static std::string kDatabaseDir;
    static const char* const kDatabaseName;

    CBLTest();
    ~CBLTest();


    CBLDatabase *db {nullptr};
};
