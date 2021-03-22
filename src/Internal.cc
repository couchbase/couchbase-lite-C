//
// Internal.cc
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

#include "Internal.hh"
#include "c4Base.hh"
#include "fleece/Fleece.h"

using namespace fleece;

namespace cbl_internal {

    void BridgeException(const char *fnName, CBLError *outError) noexcept {
        C4Error error = C4Error::fromCurrentException();
        if (outError)
            *outError = external(error);
        else
            C4WarnError("Function %s() failed: %s", fnName, error.description().c_str());
    }

    alloc_slice convertJSON5(slice json5) {
        FLStringResult errMsg;
        FLError flError;
        alloc_slice json(FLJSON5_ToJSON(json5, &errMsg, nullptr, &flError));
        if (!json) {
            alloc_slice msg(std::move(errMsg));
            C4Error::raise(FleeceDomain, flError, "%.*s", FMTSLICE(msg));
        }
        return json;
    }

}
