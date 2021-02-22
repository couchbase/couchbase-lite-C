//
// Util.cc
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

#include "Util.hh"
#include "fleece/Fleece.h"

using namespace fleece;

namespace cbl_internal {

    alloc_slice convertJSON5(slice json5, C4Error *outError) {
        FLStringResult errMsg;
        FLError flError;
        alloc_slice json(FLJSON5_ToJSON(json5, &errMsg, nullptr, &flError));
        if (!json) {
            setError(outError, FleeceDomain, flError, slice(errMsg));
            FLSliceResult_Release(errMsg);
        }
        return json;
    }

    void setError(C4Error* outError, C4ErrorDomain domain, int code, slice message) {
        if (outError)
            *outError = c4error_make(domain, code, message);
    }

}
