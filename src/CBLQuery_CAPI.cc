//
// CBLQuery_CAPI.cc
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
#include "CBLQueryTypes.h"
#include "CBLDatabase_Internal.hh"
#include "CBLCollection_Internal.hh"
#include "CBLQuery_Internal.hh"
#include <string>

using namespace std;

CBLQuery* CBLDatabase_CreateQuery(const CBLDatabase* db,
                                  CBLQueryLanguage language,
                                  FLString queryString,
                                  int *outErrorPos,
                                  CBLError* outError) noexcept
{
    try {
        auto query = db->createQuery(language, queryString, outErrorPos);
        if (!query) {
            C4Error::set(LiteCoreDomain, kC4ErrorInvalidQuery, {}, internal(outError));
            return nullptr;
        }
        return std::move(query).detach();
    } catchAndBridge(outError)
}

FLDict CBLQuery_Parameters(const CBLQuery* query) noexcept {
    return query->parameters();
}

void CBLQuery_SetParameters(CBLQuery* query, FLDict parameters) noexcept {
    query->setParameters(parameters);
}

CBLResultSet* CBLQuery_Execute(CBLQuery* query, CBLError* outError) noexcept {
    try {
        return query->execute().detach();
    } catchAndBridge(outError)
}

FLSliceResult CBLQuery_Explain(const CBLQuery* query) noexcept {
    try {
        return FLSliceResult(query->explain());
    } catchAndWarn()
}

unsigned CBLQuery_ColumnCount(const CBLQuery* query) noexcept {
    return query->columnCount();
}

FLSlice CBLQuery_ColumnName(const CBLQuery* query, unsigned col) noexcept {
    return query->columnName(col);
}

CBLListenerToken* CBLQuery_AddChangeListener(CBLQuery* query,
                                             CBLQueryChangeListener listener,
                                             void *context) noexcept
{
    return query->addChangeListener(listener, context).detach();
}

CBLResultSet* CBLQuery_CopyCurrentResults(const CBLQuery* query,
                                          CBLListenerToken *token,
                                          CBLError *outError) noexcept
{
    auto listener = query->getChangeListener(token);
    if (!listener) {
        C4Error::set(internal(outError), LiteCoreDomain, kC4ErrorNotFound,
                     "Listener token is not valid for this query");
        return nullptr;
    }
    return listener->resultSet().detach();
}

void CBLQuery_SetListenerCallbackDelay(int delayMS) noexcept {
#ifdef DEBUG
    ListenerToken<CBLQueryChangeListener>::setC4QueryObserverCallbackDelay(delayMS);
#endif
}

bool CBLResultSet_Next(CBLResultSet* rs) noexcept {
    try {
        return rs->next();
    } catchAndWarn();
}

FLValue CBLResultSet_ValueForKey(const CBLResultSet* rs, FLString property) noexcept {
    return rs->property(property);
}

FLValue CBLResultSet_ValueAtIndex(const CBLResultSet* rs, unsigned column) noexcept {
    return rs->column(column);
}

FLArray CBLResultSet_ResultArray(const CBLResultSet *rs) noexcept {
    return rs->asArray();
}

FLDict CBLResultSet_ResultDict(const CBLResultSet *rs) noexcept {
    return rs->asDict();
}

CBLQuery* CBLResultSet_GetQuery(const CBLResultSet *rs) noexcept {
    return rs->query();
}
