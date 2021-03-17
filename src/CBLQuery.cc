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


Retained<CBLResultSet> CBLQuery::execute() {
    auto qe = _c4query.use<C4Query::Enumerator>([=](C4Query *c4query) {
        return c4query->run();
    });
    return retained(new CBLResultSet(this, move(qe)));
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
                       FLString queryString,
                       int *outErrorPos,
                       CBLError* outError) CBLAPI
{
    auto query = db->createQuery(language, queryString, outErrorPos); //FIXME: Catch
    if (!query) {
        c4error_return(LiteCoreDomain, kC4ErrorInvalidQuery, {}, internal(outError));
        return nullptr;
    }
    return move(query).detach();
}

FLDict CBLQuery_Parameters(const CBLQuery* _cbl_nonnull query) CBLAPI {
    return query->parameters();
}

void CBLQuery_SetParameters(CBLQuery* query _cbl_nonnull, FLDict parameters) CBLAPI {
    query->setParameters(parameters);
}

CBLResultSet* CBLQuery_Execute(CBLQuery* query _cbl_nonnull, CBLError* outError) CBLAPI {
    return query->execute().detach();
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
    return listener->resultSet().detach();
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
