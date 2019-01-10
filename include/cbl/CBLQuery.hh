//
//  CBLQuery.hh
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
#include "CBLDatabase.hh"
#include "CBLQuery.h"

namespace cbl {
    class ResultSet;
    class ResultSetIterator;

    /** A database query. */
    class Query : protected RefCounted {
    public:
        Query(const Database& db, const char *jsonQuery _cblnonnull)
        {
            CBLError error;
            auto q = cbl_query_new(db.ref(), jsonQuery, &error);
            check(q, error);
            _ref = (CBLRefCounted*)q;
        }

        unsigned columnCount()                  {return cbl_query_columnCount(ref());}
        fleece::slice columnName(unsigned index) {return cbl_query_columnName(ref(), index);}

        void setParameters(fleece::Dict parameters)   {cbl_query_setParameters(ref(), parameters);}

        inline ResultSet execute();

        fleece::alloc_slice explain()           {return cbl_query_explain(ref());}

        [[nodiscard]] Listener addListener(std::function<void(CBLQuery, CBLResultSet, CBLError*)>);

        CBL_REFCOUNTED_BOILERPLATE(Query, RefCounted, CBLQuery)
    };


    /** A single query result; ResultSet::iterator iterates over these. */
    class Result {
    public:
        fleece::Value column(unsigned col)                  {return cbl_results_column(_ref, col);}
        fleece::Value property(const char *name _cblnonnull){return cbl_results_property(_ref, name);}

        fleece::Value operator[] (unsigned col)                 {return column(col);}
        fleece::Value operator[] (const char *name _cblnonnull) {return property(name);}

    protected:
        explicit Result(CBLResultSet *ref)    :_ref(ref) { }
        CBLResultSet* _ref;
        friend class ResultSetIterator;
    };


    /** The results of a query. The only access to the individual Results is to iterate them. */
    class ResultSet : protected RefCounted {
    public:
        using iterator = ResultSetIterator;
        inline iterator begin();
        inline iterator end();
 
        friend class Query;
        CBL_REFCOUNTED_BOILERPLATE(ResultSet, RefCounted, CBLResultSet)
    };


    // implementation of ResultSet::iterator
    class ResultSetIterator {
    public:
        Result operator->() const {return Result(_rs.ref());}

        bool operator== (const ResultSetIterator &i) const {return _rs == i._rs;}

        ResultSetIterator& operator++() {
            if (!cbl_results_next(_rs.ref()))
                _rs = ResultSet{};
            return *this;
        }
    protected:
        ResultSetIterator()                         :_rs() { }
        explicit ResultSetIterator(ResultSet rs)    :_rs(rs) { }

        ResultSet _rs;
        friend class ResultSet;
    };


    // Method implementations:

    inline ResultSet Query::execute() {
        CBLError error;
        auto rs = cbl_query_execute(ref(), &error);
        check(rs, error);
        return ResultSet(rs);
    }


    inline ResultSet::iterator ResultSet::begin()  {
        if (!_ref) throw std::logic_error("begin() can only be called once");//FIX error class
        auto i = iterator(*this);
        _ref = nullptr;
        return i;
    }

    inline ResultSet::iterator ResultSet::end() {
        return iterator();
    }

}
