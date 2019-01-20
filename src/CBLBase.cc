//
// CBLBase.cc
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

#include "CBLBase.h"
#include "Internal.hh"
#include "Util.hh"


void cbl_setLogLevel(CBLLogLevel level, CBLLogDomain domain) {
    static C4LogDomain kC4Domains[5] = {kC4DefaultLog, kC4DatabaseLog, kC4QueryLog, kC4SyncLog, kC4WebSocketLog};
    if (domain == kCBLLogDomainAll) {
        c4log_setCallbackLevel(C4LogLevel(level));
        for (int i = 0; i < 5; ++i)
            c4log_setLevel(kC4Domains[i], C4LogLevel(level));
    } else {
        c4log_setLevel(kC4Domains[domain], C4LogLevel(level));
    }
}

char* cbl_error_message(const CBLError* error _cbl_nonnull) {
    return allocCString(c4error_getMessage(*internal(error)));
}


CBLRefCounted* cbl_retain(CBLRefCounted *self) {
    return retain(self);
}


void cbl_release(CBLRefCounted *self) {
    release(self);
}


struct CBLListenerToken {
    ~CBLListenerToken() {
        // TODO
    }
};

void cbl_listener_remove(CBLListenerToken *token) {
    delete token;
}
