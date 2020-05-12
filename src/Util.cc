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
#include <stdio.h>
#include <string>

using namespace fleece;

namespace cbl_internal {

#if 0 // UNUSED
    std::string vformat(const char *fmt, va_list args) {
        char *cstr = nullptr;
        vasprintf(&cstr, fmt, args);
        std::string result(cstr);
        free(cstr);
        return result;
    }


    std::string format(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        std::string result = vformat(fmt, args);
        va_end(args);
        return result;
    }
#endif


    char* allocCString(FLSlice result) {
        if (result.buf == nullptr)
            return nullptr;
        char* str = (char*) malloc(result.size + 1);
        if (!str)
            return nullptr;
        memcpy(str, result.buf, result.size);
        str[result.size] = '\0';
        return str;
    }


    char* allocCString(FLSliceResult result) {
        char *str = allocCString(FLSlice{result.buf, result.size});
        FLSliceResult_Release(result);
        return str;
    }

    alloc_slice convertJSON5(FLSlice json5, C4Error *outError) {
        FLStringResult errMsg;
        FLError flError;
        alloc_slice json(FLJSON5_ToJSON(json5, &errMsg, nullptr, &flError));
        if (!json) {
            setError(outError, FleeceDomain, flError, slice(errMsg));
            FLSliceResult_Release(errMsg);
        }
        return json;
    }

    void setError(C4Error* outError, C4ErrorDomain domain, int code, C4String message) {
        if (outError)
            *outError = c4error_make(domain, code, message);
    }

}
