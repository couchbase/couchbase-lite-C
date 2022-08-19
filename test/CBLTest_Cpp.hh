//
// CBLTest_Cpp.hh
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
#include "cbl++/CouchbaseLite.hh"
#include "CBLTest.hh"


namespace cbl {
    // Make Catch write something better than "{?}" when it logs a CBL object:
    #define DEFINE_WRITE_OP(CLASS) \
        static inline std::ostream& operator<< (std::ostream &out, const cbl::CLASS &rc) { \
            return out << "cbl::" #CLASS "[@" << (void*)rc.ref() << "]"; \
        }
    DEFINE_WRITE_OP(Blob)
    DEFINE_WRITE_OP(Database)
    DEFINE_WRITE_OP(Document)
    DEFINE_WRITE_OP(MutableDocument)
    DEFINE_WRITE_OP(Query)
    DEFINE_WRITE_OP(Replicator)
    DEFINE_WRITE_OP(ResultSet)
}


class CBLTest_Cpp {
public:
    static const fleece::alloc_slice kDatabaseDir;
    static const fleece::slice kDatabaseName;

    CBLTest_Cpp();
    ~CBLTest_Cpp();

    cbl::Database openEmptyDatabaseNamed(fleece::slice name);
    
    cbl::Database openDatabaseNamed(fleece::slice name);
    
    void createNumberedDocs(cbl::Collection& collection, unsigned n, unsigned start = 1);

    cbl::Database db;
};
