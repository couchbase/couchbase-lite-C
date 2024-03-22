//
//  CBLPrediction.cc
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
#include "c4PredictiveQuery.h"

#ifdef COUCHBASE_ENTERPRISE

namespace cbl_internal {
    using namespace std;
    using namespace fleece;

    void PredictiveModel::registerModel(const slice name, const CBLPredictiveModel& model) {
        auto prediction = [](void* context, FLDict input, C4Database *db, C4Error *outError) {
            auto m = (PredictiveModel*)context;
            FLDict output = m->_model.prediction(m->_model.context, input);
            return C4SliceResult(m->encodeOutput(Dict(output)));
        };
        
        auto unregistered = [](void* context) {
            auto m = (PredictiveModel*)context;
            if (m->_model.unregistered) {
                m->_model.unregistered(m->_model.context);
            }
            delete m;
        };
        
        unregisterModel(name);
        
        C4PredictiveModel c4model { };
        c4model.context = new PredictiveModel(model);
        c4model.prediction = prediction;
        c4model.unregistered = unregistered;
        
        auto nameStr = name.asString();
        c4pred_registerModel(nameStr.c_str(), c4model);
    }

    void PredictiveModel::unregisterModel(slice name) {
        auto nameStr = name.asString();
        c4pred_unregisterModel(nameStr.c_str());
    }

    alloc_slice PredictiveModel::encodeOutput(Dict dict) {
        if (!dict) {
            return nullslice;
        }
        
        Encoder enc;
        enc.writeValue(dict);
        return enc.finish();
    }
}

#endif
