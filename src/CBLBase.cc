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
#include <iostream>


char* CBLError_Message(const CBLError* error _cbl_nonnull) CBLAPI {
    return allocCString(CBLError_Message_s(error));
}

FLSliceResult CBLError_Message_s(const CBLError* error _cbl_nonnull) CBLAPI {
    return c4error_getMessage(*internal(error));
}


CBLTimestamp CBL_Now(void) CBLAPI {
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
#else
    std::cerr << "(CBL_DumpInstances() is not functional in non-debug builds)\n";
#endif
}


void CBLListener_Remove(CBLListenerToken *token) CBLAPI {
    if (token)
        token->remove();
}
