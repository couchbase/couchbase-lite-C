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


// Query

CBL_REFCOUNTED(CBLQuery*, query);

/** Creates a new query.
    @param db  The database to query.
    @param jsonQuery  The query expressed in the JSON syntax.
    @param error  On failure, the error will be written here.
    @return  The new query object. */
CBLQuery* cbl_query_new(CBLDatabase* db _cblnonnull,
                        const char *jsonQuery _cblnonnull, 
                        CBLError* error);

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


// Result Set

CBL_REFCOUNTED(CBLResultSet*, results);

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


// Change Listener

typedef void (*CBLQueryListener)(void *context,
                                 CBLQuery* _cblnonnull,
                                 CBLResultSet*,
                                 CBLError*);

CBLListenerToken* cbl_query_addListener(CBLQuery* _cblnonnull,
                                        CBLQueryListener* _cblnonnull,
                                        void *context);

#ifdef __cplusplus
}
#endif
