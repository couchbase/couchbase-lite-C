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
#include "Listener.hh"
#include "Util.hh"


static const C4LogDomain kC4Domains[5] = {
    kC4DefaultLog, kC4DatabaseLog, kC4QueryLog, kC4SyncLog, kC4WebSocketLog};


void CBL_SetLogLevel(CBLLogLevel level, CBLLogDomain domain) CBLAPI {
    if (domain == kCBLLogDomainAll) {
        c4log_setCallbackLevel(C4LogLevel(level));
        for (int i = 0; i < 5; ++i)
            c4log_setLevel(kC4Domains[i], C4LogLevel(level));
    } else {
        c4log_setLevel(kC4Domains[domain], C4LogLevel(level));
    }
}


void CBL_Log(CBLLogDomain domain, CBLLogLevel level, const char *format _cbl_nonnull, ...) CBLAPI {
    char *message = nullptr;
    va_list args;
    va_start(args, format);
    vasprintf(&message, format, args);
    va_end(args);
    C4LogToAt(kC4Domains[domain], C4LogLevel(level), "%s", message);
    free(message);
}


char* CBLError_Message(const CBLError* error _cbl_nonnull) CBLAPI {
    return allocCString(c4error_getMessage(*internal(error)));
}


CBLTimestamp CBL_now(void) CBLAPI {
    return c4_now();
}


CBLRefCounted* CBL_Retain(CBLRefCounted *self) CBLAPI {
    return retain(self);
}


void CBL_Release(CBLRefCounted *self) CBLAPI {
    release(self);
}


unsigned CBL_InstanceCount() CBLAPI {
    return fleece::InstanceCounted::count();
}


void CBL_DumpInstances(void) CBLAPI {
#if INSTANCECOUNTED_TRACK
    fleece::InstanceCounted::dumpInstances();
#endif
}


void CBLListener_Remove(CBLListenerToken *token) CBLAPI {
    if (token)
        token->remove();
}
