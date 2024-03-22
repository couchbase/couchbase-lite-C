//
// CBLPrediction_CAPI.cc
//
// Copyright (C) 2024 Couchbase, Inc All rights reserved.
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

#include "CBLPrediction_Internal.hh"

#ifdef COUCHBASE_ENTERPRISE

using namespace cbl_internal;

void CBL_RegisterPredictiveModel(FLString name, CBLPredictiveModel model) noexcept {
    PredictiveModel::registerModel(name, model);
}

void CBL_UnregisterPredictiveModel(FLString name) noexcept {
    PredictiveModel::unregisterModel(name);
}

#endif
