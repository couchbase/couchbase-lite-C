//
// Util.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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
#include "fleece/slice.hh"
#include "c4Base.h"
#include <string>


#define LOCK(MUTEX)     std::lock_guard<decltype(MUTEX)> _lock(MUTEX)


namespace cbl_internal {

#if 0 // UNUSED
    /** Like sprintf(), but returns a std::string */
    std::string format(const char *fmt _cbl_nonnull, ...) __printflike(1, 2);

    /** Like vsprintf(), but returns a std::string */
    std::string vformat(const char *fmt _cbl_nonnull, va_list);
#endif
    
    char* allocCString(FLSlice);
    char* allocCString(FLSliceResult);      // frees the input

    fleece::alloc_slice convertJSON5(const char *json5, C4Error *outError);

    void setError(C4Error* outError, C4ErrorDomain domain, int code, C4String message);
}
