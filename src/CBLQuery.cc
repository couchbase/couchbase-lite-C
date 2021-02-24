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
#include <string>


Retained<CBLResultSet> CBLQuery::execute(CBLError* outError) {
    auto qe = use<C4QueryEnumerator*>([=](C4Query *c4query) {
        return c4query_run(c4query, nullptr, nullslice, internal(outError));
    });
    if (!qe)
        return nullptr;
    return make_nothrow<CBLResultSet>(outError, this, qe);
}


Retained<CBLListenerToken> CBLQuery::addChangeListener(CBLQueryChangeListener listener, void *context) {
    auto token = make_nothrow<ListenerToken<CBLQueryChangeListener>>(nullptr, this, listener, context);
    if (token) {
        _listeners.add(token);
        token->setEnabled(true);
    }
    return token;
}


#pragma mark - PUBLIC API:


CBLQuery* CBLQuery_New(const CBLDatabase* db _cbl_nonnull,
                       CBLQueryLanguage language,
                       FLString queryString,
                       int *outErrorPos,
                       CBLError* outError) CBLAPI
{
    auto query = make_nothrow<CBLQuery>(outError,
                                        db, language, queryString, outErrorPos, internal(outError));
    if (!query || !query->valid())
        return nullptr;
    return move(query).detach();
}

FLDict CBLQuery_Parameters(const CBLQuery* _cbl_nonnull query) CBLAPI {
    return query->parameters();
}

void CBLQuery_SetParameters(CBLQuery* query _cbl_nonnull, FLDict parameters) CBLAPI {
    query->setParameters(parameters);
}

CBLResultSet* CBLQuery_Execute(CBLQuery* query _cbl_nonnull, CBLError* outError) CBLAPI {
    return query->execute(outError).detach();
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
    return query->addChangeListener(listener, context).detach();
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
    return listener->resultSet(outError).detach();
}

bool CBLResultSet_Next(CBLResultSet* rs _cbl_nonnull) CBLAPI {
    return rs->next();
}

FLValue CBLResultSet_ValueForKey(const CBLResultSet* rs, FLString property) CBLAPI {
    return rs->property(property);
}

FLValue CBLResultSet_ValueAtIndex(const CBLResultSet* rs _cbl_nonnull, unsigned column) CBLAPI {
    return rs->column(column);
}

FLArray CBLResultSet_ResultArray(const CBLResultSet *rs) CBLAPI {
    return rs->asArray();
}

FLDict CBLResultSet_ResultDict(const CBLResultSet *rs) CBLAPI {
    return rs->asDict();
}

CBLQuery* CBLResultSet_GetQuery(const CBLResultSet *rs _cbl_nonnull) CBLAPI {
    return rs->query();
}


#pragma mark - INDEXES:


bool CBLDatabase_CreateIndex(CBLDatabase *db _cbl_nonnull,
                             FLString name,
                             CBLIndexSpec spec,
                             CBLError *outError) CBLAPI
{
    C4IndexOptions options = {};
    options.ignoreDiacritics = spec.ignoreAccents;
    string languageStr;
    if (spec.language.buf) {
        languageStr = string(spec.language);
        options.language = languageStr.c_str();
    }
    return db->use<bool>([&](C4Database *c4db) {
        return c4db_createIndex(c4db,
                                name,
                                spec.keyExpressionsJSON,
                                (C4IndexType)spec.type,
                                &options,
                                internal(outError));
    });
}

bool CBLDatabase_DeleteIndex(CBLDatabase *db _cbl_nonnull,
                             FLString name,
                             CBLError *outError) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        return c4db_deleteIndex(c4db, name, internal(outError));
    });
}

FLMutableArray CBLDatabase_IndexNames(CBLDatabase *db _cbl_nonnull) CBLAPI {
    return db->use<FLMutableArray>([&](C4Database *c4db) {
        Doc doc(alloc_slice(c4db_getIndexesInfo(c4db, nullptr)));
        MutableArray indexes = MutableArray::newArray();
        for (Array::iterator i(doc.root().asArray()); i; ++i) {
            Dict info = i.value().asDict();
            indexes.append(info["name"]);
        }
        return FLMutableArray_Retain(indexes);
    });
}

