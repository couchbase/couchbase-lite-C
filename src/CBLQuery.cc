//
// CBLQuery.cc
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

#include "CBLQuery.h"
#include "Internal.hh"
#include "c4.hh"
#include "c4Query.h"
#include "fleece/Fleece.hh"
#include <unordered_map>

using namespace std;
using namespace fleece;


class CBLQuery : public CBLRefCounted {
public:

    CBLQuery(const CBLDatabase* db _cblnonnull,
             const char *jsonQuery _cblnonnull,
             C4Error* outError)
    :_c4query( c4query_new(internal(db), slice(jsonQuery), outError) )
    { }

    bool valid() const {
        return _c4query != nullptr;
    }

    void setParameters(FLSlice encodedParameters) {
        _parameters = encodedParameters;
    }

    Retained<CBLResultSet> execute(C4Error* outError);

    alloc_slice explain() {
        return c4query_explain(_c4query);
    }

    unsigned columnCount() {
        return c4query_columnCount(_c4query);
    }

    slice columnName(unsigned col) {
        return c4query_columnTitle(_c4query, col);
    }

    int columnNamed(slice name) {
        if (!_columnNames) {
            _columnNames.reset(new std::unordered_map<slice, uint32_t>);
            unsigned nCols = columnCount();
            _columnNames->reserve(nCols);
            for (unsigned col = 0; col < nCols; ++col)
                _columnNames->insert({columnName(col), col});
        }
        auto i = _columnNames->find(name);
        return (i != _columnNames->end()) ? i->second : -1;
    }

private:
    c4::ref<C4Query> const _c4query;
    alloc_slice _parameters;
    unique_ptr<std::unordered_map<slice, unsigned>> _columnNames;
};


class CBLResultSet : public CBLRefCounted {
public:
    CBLResultSet(CBLQuery* query, C4QueryEnumerator* qe _cblnonnull)
    :_query(query)
    ,_enum(qe)
    { }

    bool next() {
        return c4queryenum_next(_enum, nullptr);
    }

    Value property(const char *prop) {
        int col = _query->columnNamed(slice(prop));
        return (col >= 0) ? column(col) : nullptr;
    }

    Value column(unsigned col) {
        return FLArrayIterator_GetValueAt(&_enum->columns, uint32_t(col));
    }

private:
    Retained<CBLQuery> const _query;
    c4::ref<C4QueryEnumerator> const _enum;
};


Retained<CBLResultSet> CBLQuery::execute(C4Error* outError) {
    C4QueryOptions options = {};
    auto qe = c4query_run(_c4query, &options, _parameters, outError);
    return qe ? retained(new CBLResultSet(this, qe)) : nullptr;
}


#pragma mark - PUBLIC API:


CBLQuery* cbl_query_new(const CBLDatabase* db _cblnonnull,
                        const char *jsonQuery _cblnonnull,
                        CBLError* outError)
{
    auto query = retained(new CBLQuery(db, jsonQuery, internal(outError)));
    return query->valid() ? retain(query.get()) : nullptr;
}

void cbl_query_setParameters(CBLQuery* query _cblnonnull, FLDict parameters) {
    Encoder enc;
    enc.writeValue(Dict(parameters));
    alloc_slice encodedParameters = enc.finish();
    query->setParameters(encodedParameters);
}

CBLResultSet* cbl_query_execute(CBLQuery* query _cblnonnull, CBLError* outError) {
    return query->execute(internal(outError));
}

FLSliceResult cbl_query_explain(CBLQuery* query _cblnonnull) {
    return FLSliceResult(query->explain());
}

unsigned cbl_query_columnCount(CBLQuery* query _cblnonnull) {
    return query->columnCount();
}

FLSlice cbl_query_columnName(CBLQuery* query _cblnonnull, unsigned col) {
    return query->columnName(col);
}


bool cbl_results_next(CBLResultSet* rs _cblnonnull) {
    return rs->next();
}

FLValue cbl_results_property(CBLResultSet* rs _cblnonnull, const char *property) {
    return rs->property(property);
}

FLValue cbl_results_column(CBLResultSet* rs _cblnonnull, unsigned column) {
    return rs->column(column);
}
