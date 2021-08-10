//
// CBLDatabase+Android.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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

#include "CBLDatabase_Internal.hh"
#include "betterassert.hh"
#include "FilePath.hh"

std::string CBLDatabase::defaultDirectory() {
    auto context = getInitContext();
    if (!context) {
        C4Error::raise(LiteCoreDomain, kC4ErrorUnsupported,
                       "The default directory is not found as the context hasn't been initialized. Call \r CBL_Init to initialize the context.");
    }
    
    litecore::FilePath dir(context->filesDir, "");
    return dir["CouchbaseLite"].path();
}
