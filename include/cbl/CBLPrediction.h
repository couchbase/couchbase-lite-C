//
// CBLPrediction.h
//
// Copyright (c) 2024 Couchbase, Inc All rights reserved.
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

#ifdef COUCHBASE_ENTERPRISE

CBL_CAPI_BEGIN

/** Predictive Model  */
typedef struct {
    /** A pointer to any external data needed by the `prediction` callback, which will receive this as its first parameter. */
    void* _cbl_nullable context;
    
    /** Called from within a query (or document indexing) to run the prediction.
        @param context  The value of the CBLPredictiveModel's `context` field.
        @param input  The input dictionary from the query.
        @return The output dictionary of the prediction function or NULL if there is no output.
        @note The output dictionary will be automatically released after it's being consumed.
        @warning This function must be "pure": given the same input parameters it must always
                 produce the same output (otherwise indexes or queries may be messed up).
                 It MUST NOT alter the database or any documents, nor run a query: either of
                 those are very likely to cause a crash. */
    FLDict _cbl_nullable (* _cbl_nonnull prediction)(void* _cbl_nullable context, FLDict input);

    /** Called if the model is unregistered, so it can release resources. */
    void (*_cbl_nullable unregistered)(void* context);
} CBLPredictiveModel;

void CBL_RegisterPredictiveModel(FLString name, CBLPredictiveModel model) CBLAPI;

void CBL_UnregisterPredictiveModel(FLString name) CBLAPI;

CBL_CAPI_END

#endif
