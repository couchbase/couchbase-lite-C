//
// CBLListener.h
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup listeners   Listeners
     @{
    Every API function that registers a listener callback returns an opaque token representing
    the registered callback. To unregister any type of listener, call \ref cbl_listener_remove.
 */

/** An opaque 'cookie' representing a registered listener callback.
    It's returned from functions that register listeners, and used to remove a listener. */
typedef struct CBLListenerToken CBLListenerToken;

/** Removes a listener callback, given the token that was returned when it was added. */
void cbl_listener_remove(CBLListenerToken*);


    typedef struct CBLSpeaker CBLSpeaker;
    CBL_REFCOUNTED(CBLSpeaker, "speaker")


    

/** @} */



#ifdef __cplusplus
}
#endif
