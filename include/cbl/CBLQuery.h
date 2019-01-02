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

#include "CBLBase.h"
#include "fleece/Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup queries   Queries
    @{ */

/** \name  Query objects
    @{ */

/** Creates a new query.
    @param db  The database to query.
    @param jsonQuery  The query expressed in the JSON syntax.
    @param error  On failure, the error will be written here.
    @return  The new query object. */
CBLQuery* cbl_query_new(const CBLDatabase* db _cblnonnull,
                        const char *jsonQuery _cblnonnull, 
                        CBLError* error);

CBL_REFCOUNTED(CBLQuery*, query);

/** Assigns values to the query's parameters. */
void cbl_query_setParameters(CBLQuery* _cblnonnull query,
                             FLDict _cblnonnull parameters);

/** Runs the query, returning the results. */
CBLResultSet* cbl_query_execute(CBLQuery* _cblnonnull, CBLError*);

/** Returns information about the query, including the translated SQL form, and the search
    strategy. You can use this to help optimize the query. */
FLSliceResult cbl_query_explain(CBLQuery* _cblnonnull);

/** Returns the number of columns in each result. */
unsigned cbl_query_columnCount(CBLQuery* _cblnonnull);

/** Returns the name of a column in the result. */
FLSlice cbl_query_columnName(CBLQuery* _cblnonnull,
                             unsigned columnIndex);

/** @} */



/** \name  Result sets
    @{
    A `CBLResultSet` is an iterator over the results returned by a query. It exposes one
    result at a time -- as a collection of values indexed either by position or by name --
    and can be stepped from one result to the next.
 */

/** Moves the result-set iterator to the next result.
    Returns false if there are no more results.
    @note This must be called _before_ examining the first result. */
bool cbl_results_next(CBLResultSet* _cblnonnull);

/** Returns the value of a column of the current result, given its (zero-based) numeric index. */
FLValue cbl_results_column(CBLResultSet* _cblnonnull,
                           unsigned column);

/** Returns the value of a column of the current result, given its name. */
FLValue cbl_results_property(CBLResultSet* _cblnonnull,
                             const char* property _cblnonnull);

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
typedef void (*CBLQueryListener)(void *context,
                                 CBLQuery* query _cblnonnull,
                                 CBLResultSet* newResults,
                                 const CBLError* error);

/** Registers a change listener callback with a query, turning it into a "live query" until
    the listener is removed (via `cbl_listener_remove`).
    @param query  The query to observe.
    @param listener  The callback to be invoked.
    @param context  An opaque value that will be passed to the callback.
    @return  A token to be passed to `cbl_listener_remove` when it's time to remove the
            listener.*/
CBLListenerToken* cbl_query_addListener(CBLQuery* query _cblnonnull,
                                        CBLQueryListener* listener _cblnonnull,
                                        void *context);

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif
