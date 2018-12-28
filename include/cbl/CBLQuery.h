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

CBLQuery* cbl_query_new(CBLDatabase* _cblnonnull, 
                        const char *jsonQuery _cblnonnull, 
                        CBLError*);

void cbl_query_setParameters(CBLQuery* _cblnonnull, FLSlice encodedParameters);

CBLResultSet* cbl_query_execute(CBLQuery* _cblnonnull, CBLError*);

FLSliceResult cbl_query_explain(CBLQuery* _cblnonnull);

unsigned cbl_query_columnCount(CBLQuery* _cblnonnull);

FLSlice cbl_query_columnName(CBLQuery* _cblnonnull, unsigned col);


// Change Listener

typedef void (*CBLQueryListener)(void *context, 
                                 CBLQuery* _cblnonnull, 
                                 CBLResultSet*, 
                                 CBLError*);

CBLListenerToken* cbl_query_addListener(CBLQuery* _cblnonnull, 
                                        CBLQueryListener* _cblnonnull,
                                        void *context);


// Result Set

CBL_REFCOUNTED(CBLResultSet*, results);

bool cbl_results_next(CBLResultSet* _cblnonnull);

FLValue cbl_results_column(CBLResultSet* _cblnonnull, unsigned column);

FLValue cbl_results_property(CBLResultSet* _cblnonnull, const char* property _cblnonnull);

#ifdef __cplusplus
}
#endif
