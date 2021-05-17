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
#include "CBLPrivate.h"
#include "Internal.hh"
#include "Listener.hh"
#include <iostream>


static_assert(sizeof(CBLError) == sizeof(C4Error));


FLSliceResult CBLError_Message(const CBLError* error) noexcept {
    return c4error_getMessage(internal(*error));
}

FLSliceResult CBLError_Description(const CBLError* error) noexcept {
    return c4error_getDescription(internal(*error));
}

void CBLError_SetCaptureBacktraces(bool capture) noexcept {
    C4Error::setCaptureBacktraces(capture);
}

bool CBLError_GetCaptureBacktraces(void) noexcept {
    return C4Error::getCaptureBacktraces();
}



CBLTimestamp CBL_Now(void) noexcept {
    return c4_now();
}


CBLRefCounted* CBL_Retain(CBLRefCounted *self) noexcept {
    return retain(self);
}


void CBL_Release(CBLRefCounted *self) noexcept {
    release(self);
}


unsigned CBL_InstanceCount() noexcept {
    return fleece::InstanceCounted::liveInstanceCount();
}


void CBL_DumpInstances(void) noexcept {
#if INSTANCECOUNTED_TRACK
    fleece::InstanceCounted::dumpInstances();
#else
    std::cerr << "(CBL_DumpInstances() is not functional in non-debug builds)\n";
#endif
}


void CBLListener_Remove(CBLListenerToken *token) noexcept {
    if (token) {
        token->remove();
        release(token);
    }
}
