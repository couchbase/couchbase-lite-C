//
// CBLQuery.h
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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
#include "CBLBase.h"
#include "fleece/Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup queries   Queries
    @{
    A CBLQuery represents a compiled N1QL-like database query.
    The C API does not have the complex class structure that provides the "fluent" query builder
    API in object-oriented languages; instead, queries are described using the raw
    [JSON schema](https://github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema)
    understood by the core query engine.
 */

/** \name  Query objects
    @{ */

/** Creates a new query.
    @note  You must release the query when you're finished with it.
    @param db  The database to query.
    @param jsonQuery  The query expressed in the [JSON schema](https://github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema). You may use JSON5 syntax.
    @param error  On failure, the error will be written here.
    @return  The new query object. */
_cbl_warn_unused
CBLQuery* cbl_query_new(const CBLDatabase* db _cbl_nonnull,
                        const char *jsonQuery _cbl_nonnull, 
                        CBLError* error) CBLAPI;

CBL_REFCOUNTED(CBLQuery*, query);

/** Assigns values to the query's parameters.
    These values will be substited for those parameters the next time the query is executed.
    @param query  The query.
    @param parameters  The parameters in the form of a Fleece \ref FLDict "dictionary" whose
            keys are the parameter names. (It's easiest to construct this by using the mutable
            API, i.e. calling \ref FLMutableDict_New and adding keys/values.) */
void cbl_query_setParameters(CBLQuery* _cbl_nonnull query,
                             FLDict _cbl_nonnull parameters) CBLAPI;

/** Returns the query's current parameter bindings, if any. */
FLDict cbl_query_parameters(CBLQuery* _cbl_nonnull query) CBLAPI;

/** Assigns values to the query's parameters, from JSON data.
    These values will be substited for those parameters the next time the query is executed.
    @param query  The query.
    @param json  The parameters in the form of a JSON-encoded object whose
            keys are the parameter names. (You may use JSON5 syntax.) */
bool cbl_query_setParametersAsJSON(CBLQuery* _cbl_nonnull query,
                                   const char* _cbl_nonnull json) CBLAPI;

/** Runs the query, returning the results.
    @note  You must release the result set when you're finished with it. */
_cbl_warn_unused
CBLResultSet* cbl_query_execute(CBLQuery* _cbl_nonnull, CBLError*) CBLAPI;

/** Returns information about the query, including the translated SQL form, and the search
    strategy. You can use this to help optimize the query. */
FLSliceResult cbl_query_explain(CBLQuery* _cbl_nonnull) CBLAPI;

/** Returns the number of columns in each result. */
unsigned cbl_query_columnCount(CBLQuery* _cbl_nonnull) CBLAPI;

/** Returns the name of a column in the result. */
FLSlice cbl_query_columnName(CBLQuery* _cbl_nonnull,
                             unsigned columnIndex) CBLAPI;

/** @} */



/** \name  Result sets
    @{
    A `CBLResultSet` is an iterator over the results returned by a query. It exposes one
    result at a time -- as a collection of values indexed either by position or by name --
    and can be stepped from one result to the next.
 */

/** Moves the result-set iterator to the next result.
    Returns false if there are no more results.
    @warning This must be called _before_ examining the first result. */
bool cbl_resultset_next(CBLResultSet* _cbl_nonnull) CBLAPI;

/** Returns the value of a column of the current result, given its (zero-based) numeric index. */
FLValue cbl_resultset_valueAtIndex(CBLResultSet* _cbl_nonnull,
                                   unsigned index) CBLAPI;

/** Returns the value of a column of the current result, given its name. */
FLValue cbl_resultset_valueForKey(CBLResultSet* _cbl_nonnull,
                                  const char* key _cbl_nonnull) CBLAPI;

CBL_REFCOUNTED(CBLResultSet*, results);

/** @} */



/** \name  Change listener
    @{
    Adding a change listener to a query turns it into a "live query". When changes are made to
    documents, the query will periodically re-run and compare its results with the prior
    results; if the new results are different, the listener callback will be called.

    @note  The result set passed to the listener is the _entire new result set_, not just the
            rows that changed.
 */

/** A callback to be invoked after the query's results have changed.
    @param context  The same `context` value that you passed when adding the listener.
    @param query  The query that triggered the listener.
    @param newResults  The entire new result set, or NULL if there was an error.
    @param error  The error that occurred, or NULL if the query ran successfully. */
typedef void (*CBLQueryChangeListener)(void *context,
                                       CBLQuery* query _cbl_nonnull,
                                       CBLResultSet* newResults,
                                       const CBLError* error);

/** Registers a change listener callback with a query, turning it into a "live query" until
    the listener is removed (via \ref cbl_listener_remove).
    @param query  The query to observe.
    @param listener  The callback to be invoked.
    @param context  An opaque value that will be passed to the callback.
    @return  A token to be passed to \ref cbl_listener_remove when it's time to remove the
            listener.*/
_cbl_warn_unused
CBLListenerToken* cbl_query_addChangeListener(CBLQuery* query _cbl_nonnull,
                                              CBLQueryChangeListener* listener _cbl_nonnull,
                                              void *context) CBLAPI;

/** @} */



/** \name  Database Indexes
    @{
 */

/** Types of database indexes. */
typedef CBL_ENUM(uint32_t, CBLIndexType) {
    kCBLValueIndex,
    kCBLFullTextIndex
};

/** Parameters for creating a database index. */
typedef struct {
    CBLIndexType    type;
    const char*     keyExpressionsJSON;
    bool            ignoreAccents;
    const char*     language;
} CBLIndexSpec;

/** Creates a database index.
    If an identical index with that name already exists, nothing happens (and no error is returned.)
    If a non-identical index with that name already exists, it is deleted and re-created. */
bool cbl_db_createIndex(CBLDatabase *db _cbl_nonnull,
                        const char* name _cbl_nonnull,
                        CBLIndexSpec,
                        CBLError *outError) CBLAPI;

/** Deletes an index given its name. */
bool cbl_db_deleteIndex(CBLDatabase *db _cbl_nonnull,
                        const char *name _cbl_nonnull,
                        CBLError *outError) CBLAPI;

/** Returns the names of the indexes on this database, as an array of strings.
    @note  You are responsible for releasing the returned Fleece array. */
FLMutableArray cbl_db_indexNames(CBLDatabase *db _cbl_nonnull) CBLAPI;

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif
