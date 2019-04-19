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
#include "CBLDatabase_Internal.hh"
#include "Internal.hh"
#include "Util.hh"
#include "c4.hh"
#include "c4Query.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <unordered_map>

using namespace std;
using namespace fleece;


class CBLQuery : public CBLRefCounted {
public:

    CBLQuery(const CBLDatabase* db _cbl_nonnull,
             CBLQueryLanguage language,
             const char *queryString _cbl_nonnull,
             C4Error* outError)
    {
        alloc_slice json;
        switch (language) {
            case kCBLN1QLLanguage:
                json = c4query_translateN1QL(slice(queryString), nullptr, outError);
                break;
            case kCBLJSONLanguage:
                json = convertJSON5(queryString, outError);
                break;
        }
        if (json)
            _c4query = c4query_new(internal(db), json, outError);
    }

    bool valid() const                              {return _c4query != nullptr;}
    FLSlice parameters() const                      {return _parameters;}
    void setParameters(FLSlice encodedParameters)   {_parameters = encodedParameters;}
    alloc_slice explain() const                     {return c4query_explain(_c4query);}
    unsigned columnCount() const                    {return c4query_columnCount(_c4query);}
    slice columnName(unsigned col) const            {return c4query_columnTitle(_c4query, col);}

    Retained<CBLResultSet> execute(C4Error* outError);

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
    c4::ref<C4Query> _c4query;
    alloc_slice _parameters;
    unique_ptr<std::unordered_map<slice, unsigned>> _columnNames;
};


class CBLResultSet : public CBLRefCounted {
public:
    CBLResultSet(CBLQuery* query, C4QueryEnumerator* qe _cbl_nonnull)
    :_query(query)
    ,_enum(qe)
    { }

    bool next() {
        C4Error error;
        bool more = c4queryenum_next(_enum, &error);
        if (!more && error.code != 0)
            C4LogToAt(kC4QueryLog, kC4LogWarning,
                      "cbl_result_next: got error %d/%d", error.domain, error.code);
        return more;
    }

    Value property(const char *prop) {
        int col = _query->columnNamed(slice(prop));
        return (col >= 0) ? column(col) : nullptr;
    }

    Value column(unsigned col) {
        if (col < 64 && (_enum->missingColumns & (1ULL<<col)))
            return nullptr;
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


CBLQuery* CBLQuery_New(const CBLDatabase* db _cbl_nonnull,
                       CBLQueryLanguage language,
                       const char *queryString _cbl_nonnull,
                       CBLError* outError) CBLAPI
{
    auto query = retained(new CBLQuery(db, language, queryString, internal(outError)));
    return query->valid() ? retain(query.get()) : nullptr;
}

FLDict CBLQuery_Parameters(CBLQuery* _cbl_nonnull query) CBLAPI {
    return FLValue_AsDict(FLValue_FromData(query->parameters(), kFLTrusted));
}

void CBLQuery_SetParameters(CBLQuery* query _cbl_nonnull, FLDict parameters) CBLAPI {
    Encoder enc;
    enc.writeValue(Dict(parameters));
    alloc_slice encodedParameters = enc.finish();
    query->setParameters(encodedParameters);
}

bool CBLQuery_SetParametersAsJSON(CBLQuery* query, const char* json5) CBLAPI {
    alloc_slice json = convertJSON5(json5, nullptr);
    if (!json)
        return false;
    Encoder enc;
    enc.convertJSON(json);
    alloc_slice encodedParameters = enc.finish();
    if (!encodedParameters)
        return false;
    query->setParameters(encodedParameters);
    return true;
}

CBLResultSet* CBLQuery_Execute(CBLQuery* query _cbl_nonnull, CBLError* outError) CBLAPI {
    return retain(query->execute(internal(outError)).get());
}

FLSliceResult CBLQuery_Explain(CBLQuery* query _cbl_nonnull) CBLAPI {
    return FLSliceResult(query->explain());
}

unsigned CBLQuery_ColumnCount(CBLQuery* query _cbl_nonnull) CBLAPI {
    return query->columnCount();
}

FLSlice CBLQuery_ColumnName(CBLQuery* query _cbl_nonnull, unsigned col) CBLAPI {
    return query->columnName(col);
}


bool CBLResultSet_Next(CBLResultSet* rs _cbl_nonnull) CBLAPI {
    return rs->next();
}

FLValue CBLResultSet_ValueForKey(CBLResultSet* rs _cbl_nonnull, const char *property) CBLAPI {
    return rs->property(property);
}

FLValue CBLResultSet_ValueAtIndex(CBLResultSet* rs _cbl_nonnull, unsigned column) CBLAPI {
    return rs->column(column);
}


#pragma mark - INDEXES:


bool CBLDatabase_CreateIndex(CBLDatabase *db _cbl_nonnull,
                        const char* name _cbl_nonnull,
                        CBLIndexSpec spec,
                        CBLError *outError) CBLAPI
{
    C4IndexOptions options = {};
    options.language = spec.language;
    options.ignoreDiacritics = spec.ignoreAccents;
    return c4db_createIndex(internal(db),
                            slice(name),
                            slice(spec.keyExpressionsJSON),
                            (C4IndexType)spec.type,
                            &options,
                            internal(outError));
}

bool CBLDatabase_DeleteIndex(CBLDatabase *db _cbl_nonnull,
                        const char *name _cbl_nonnull,
                        CBLError *outError) CBLAPI
{
    return c4db_deleteIndex(internal(db), slice(name), internal(outError));
}

FLMutableArray CBLDatabase_IndexNames(CBLDatabase *db _cbl_nonnull) CBLAPI {
    Doc doc(alloc_slice(c4db_getIndexes(internal(db), nullptr)));
    MutableArray indexes = doc.root().asArray().mutableCopy(kFLDeepCopyImmutables);
    return FLMutableArray_Retain(indexes);
}

