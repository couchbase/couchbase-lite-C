//
// CBLReplicator.h
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

#include "CBLBase.h"
#include "fleece/FLSlice.h"

#ifdef __cplusplus
extern "C" {
#endif


// Configuration

typedef CBL_ENUM(uint8_t, CBLReplicatorType) { ... };

typedef struct {
    CBLDatabase* database;
    CBLEndpoint* endpoint;
    CBLReplicatorType replicatorType;
    bool continuous;
    CBLAuthenticator* authenticator;
    FLSlice pinnedServerCertificate;
    FLDict headers;
    const char **channels;          // i.e. ptr to C array of C strings, ending with NULL
    const char **documentIDs;
} CBLReplicatorConfiguration;


// Replicator

CBL_REFCOUNTED(CBLReplicator*, repl);

cbl_repl_new(CBLReplicatorConfiguration* _cblnonnull);

const CBLReplicatorConfiguration* cbl_repl_config(CBLReplicator* _cblnonnull);

void cbl_repl_start(CBLReplicator* _cblnonnull);
void cbl_repl_stop(CBLReplicator* _cblnonnull);

void cbl_repl_resetCheckpoint(CBLReplicator* _cblnonnull);


// Status

typedef CBL_ENUM(uint8_t, CBLReplicatorActivityLevel) { ... };

typedef struct {
    uint64_t completed, total;
} CBLReplicatorProgress;

typedef struct {
    CBLReplicatorActivityLevel activity;
    CBLReplicatorProgress progress;
    CBLError error;
} CBLReplicatorStatus;
    
const CBLReplicatorStatus* cbl_repl_status(CBLReplicator*);


// Change Listener

typedef void (*CBLReplicatorListener)(void *context, 
                                      CBLReplicator* _cblnonnull, 
                                      const CBLReplicatorStatus* _cblnonnull);

CBLListenerToken* cbl_repl_addListener(CBLReplicator* _cblnonnull,
                                       CBLReplicatorListener _cblnonnull, 
                                       void *context);

#ifdef __cplusplus
}
#endif
