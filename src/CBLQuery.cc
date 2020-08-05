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
#include "CBLQuery_Internal.hh"


Retained<CBLResultSet> CBLQuery::execute(C4Error* outError) {
    auto qe = use<C4QueryEnumerator*>([=](C4Query *c4query) {
        return c4query_run(c4query, nullptr, nullslice, outError);
    });
    return qe ? retained(new CBLResultSet(this, qe)) : nullptr;
}


Retained<CBLListenerToken> CBLQuery::addChangeListener(CBLQueryChangeListener listener, void *context) {
    auto token = retained(new ListenerToken<CBLQueryChangeListener>(this, listener, context));
    _listeners.add(token);
    token->setEnabled(true);
    return token;
}


#pragma mark - PUBLIC API:


CBLQuery* CBLQuery_New(const CBLDatabase* db _cbl_nonnull,
                       CBLQueryLanguage language,
                       const char *queryString _cbl_nonnull,
                       int *outErrorPos,
                       CBLError* outError) CBLAPI
{
    return CBLQuery_New_s(db, language, slice(queryString), outErrorPos, outError);
}

CBLQuery* CBLQuery_New_s(const CBLDatabase* db _cbl_nonnull,
                         CBLQueryLanguage language,
                         FLString queryString,
                         int *outErrorPos,
                         CBLError* outError) CBLAPI
{
    auto query = retained(new CBLQuery(db, language, queryString, outErrorPos, internal(outError)));
    return query->valid() ? retain(query) : nullptr;
}

FLDict CBLQuery_Parameters(const CBLQuery* _cbl_nonnull query) CBLAPI {
    return query->parameters();
}

void CBLQuery_SetParameters(CBLQuery* query _cbl_nonnull, FLDict parameters) CBLAPI {
    query->setParameters(parameters);
}

bool CBLQuery_SetParametersAsJSON(CBLQuery* query, const char* json5) CBLAPI {
    return CBLQuery_SetParametersAsJSON_s(query, slice(json5));
}

bool CBLQuery_SetParametersAsJSON_s(CBLQuery* query, FLString json5) CBLAPI {
    query->setParametersAsJSON(json5);
    return true;
}

CBLResultSet* CBLQuery_Execute(CBLQuery* query _cbl_nonnull, CBLError* outError) CBLAPI {
    return retain(query->execute(internal(outError)));
}

FLSliceResult CBLQuery_Explain(const CBLQuery* query _cbl_nonnull) CBLAPI {
    return FLSliceResult(query->explain());
}

unsigned CBLQuery_ColumnCount(const CBLQuery* query _cbl_nonnull) CBLAPI {
    return query->columnCount();
}

FLSlice CBLQuery_ColumnName(const CBLQuery* query _cbl_nonnull, unsigned col) CBLAPI {
    return query->columnName(col);
}

CBLListenerToken* CBLQuery_AddChangeListener(CBLQuery* query _cbl_nonnull,
                                             CBLQueryChangeListener listener _cbl_nonnull,
                                             void *context) CBLAPI
{
    return retain(query->addChangeListener(listener, context));
}

CBLResultSet* CBLQuery_CopyCurrentResults(const CBLQuery* query,
                                          CBLListenerToken *token,
                                          CBLError *outError) CBLAPI
{
    auto listener = query->getChangeListener(token);
    if (!listener) {
        setError(internal(outError), LiteCoreDomain, kC4ErrorNotFound,
                 "Listener token is not valid for this query"_sl);
        return nullptr;
    }
    return retain(listener->resultSet(outError));
}

bool CBLResultSet_Next(CBLResultSet* rs _cbl_nonnull) CBLAPI {
    return rs->next();
}

FLValue CBLResultSet_ValueForKey(const CBLResultSet* rs _cbl_nonnull, const char *property) CBLAPI {
    return CBLResultSet_ValueForKey_s(rs, slice(property));
}

FLValue CBLResultSet_ValueForKey_s(const CBLResultSet* rs, FLString property) CBLAPI {
    return rs->property(property);
}

FLValue CBLResultSet_ValueAtIndex(const CBLResultSet* rs _cbl_nonnull, unsigned column) CBLAPI {
    return rs->column(column);
}

FLArray CBLResultSet_RowArray(const CBLResultSet *rs) CBLAPI {
    return rs->asArray();
}

FLDict CBLResultSet_RowDict(const CBLResultSet *rs) CBLAPI {
    return rs->asDict();
}

CBLQuery* CBLResultSet_GetQuery(const CBLResultSet *rs _cbl_nonnull) CBLAPI {
    return rs->query();
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
    return db->use<bool>([&](C4Database *c4db) {
        return c4db_createIndex(c4db,
                                slice(name),
                                slice(spec.keyExpressionsJSON),
                                (C4IndexType)spec.type,
                                &options,
                                internal(outError));
    });
}

bool CBLDatabase_CreateIndex_s(CBLDatabase *db,
                               FLString name,
                               CBLIndexSpec_s spec_s,
                               CBLError *outError) CBLAPI
{
    string json(slice(spec_s.keyExpressionsJSON));
    CBLIndexSpec spec = {spec_s.type, json.c_str(), spec_s.ignoreAccents};
    string language;
    if (spec_s.language.buf) {
        language = slice(spec.language);
        spec.language = language.c_str();
    }
    return CBLDatabase_CreateIndex(db, string(slice(name)).c_str(), spec, outError);
}

bool CBLDatabase_DeleteIndex(CBLDatabase *db _cbl_nonnull,
                        const char *name _cbl_nonnull,
                        CBLError *outError) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        return c4db_deleteIndex(c4db, slice(name), internal(outError));
    });
}

FLMutableArray CBLDatabase_IndexNames(CBLDatabase *db _cbl_nonnull) CBLAPI {
    return db->use<FLMutableArray>([&](C4Database *c4db) {
        Doc doc(alloc_slice(c4db_getIndexes(c4db, nullptr)));
        MutableArray indexes = doc.root().asArray().mutableCopy(kFLDeepCopyImmutables);
        return FLMutableArray_Retain(indexes);
    });
}

