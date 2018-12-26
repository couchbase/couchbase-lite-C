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
#include <stdio.h>
#include <string>

namespace cbl {

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

}
