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

#pragma once
#include "CBLBase.h"
#include "fleece/Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup replication   Replication
    A replicator is a background task that synchronizes changes between a local database and
    another database on a remote server (or on a peer device, or even another local database.)
    @{ */

/** \name  Configuration
    @{ */

/** An opaque object representing a database to replicate with. */
typedef struct CBLEndpoint CBLEndpoint;

/** Creates a new endpoint representing a server at the given URL.
    The URL's scheme must be `ws` or `wss`, it must of course have a valid hostname,
    and its path must be the name of the database on that server.
    The port can be omitted; it defaults to 80 for `ws` and 443 for `wss`.
    For example: `wss://example.org/dbname` */
CBLEndpoint* cblendpoint_newWithURL(const char *url _cbl_nonnull) CBLAPI;


#ifdef COUCHBASE_ENTERPRISE
/** Creates a new endpoint representing another local database. */
CBLEndpoint* cblendpoint_newWithLocalDB(CBLDatabase* _cbl_nonnull) CBLAPI;
#endif

/** Frees a CBLEndpoint object. */
void cblendpoint_free(CBLEndpoint*) CBLAPI;


/** An opaque object representing authentication credentials for a remote server. */
typedef struct CBLAuthenticator CBLAuthenticator;

/** Creates an authenticator for HTTP Basic (username/password) auth. */
CBLAuthenticator* cblauth_newBasic(const char *username _cbl_nonnull,
                                   const char *password _cbl_nonnull) CBLAPI;

/** Frees a CBLAuthenticator object. */
void cblauth_free(CBLAuthenticator*) CBLAPI;


/** Direction of replication: push, pull, or both. */
typedef CBL_ENUM(uint8_t, CBLReplicatorType) {
    kCBLReplicatorTypePushAndPull = 0,    ///< Bidirectional; both push and pull
    kCBLReplicatorTypePush,               ///< Pushing changes to the target
    kCBLReplicatorTypePull                ///< Pulling changes from the target
};

/** A callback that can decide whether a particular document should be pushed or pulled. */
typedef bool (*CBLReplicationFilter)(void *context, CBLDocument* document, bool isDeleted);


/** The configuration of a replicator. */
typedef struct {
    CBLDatabase* database;              ///< The database to replicate
    CBLEndpoint* endpoint;              ///< The address of the other database to replicate with
    CBLReplicatorType replicatorType;   ///< Push, pull or both
    bool continuous;                    ///< Continuous replication?
    CBLAuthenticator* authenticator;    ///< Authentication credentials, if needed
    FLSlice pinnedServerCertificate;    ///< An X.509 cert to "pin" TLS connections to
    FLDict headers;                     ///< Extra HTTP headers to add to the WebSocket request
    FLArray channels;                   ///< Optional set of channels to pull from
    FLArray documentIDs;                ///< Optional set of document IDs to replicate
    CBLReplicationFilter pushFilter;    ///< Optional callback to filter which docs are pushed
    CBLReplicationFilter pullFilter;    ///< Optional callback to validate incoming docs
    void* filterContext;                ///< Arbitrary value passed to filter callbacks
} CBLReplicatorConfiguration;

/** @} */



/** \name  Lifecycle
    @{ */

CBL_REFCOUNTED(CBLReplicator*, repl);

/** Creates a replicator with the given configuration. */
CBLReplicator* cbl_repl_new(const CBLReplicatorConfiguration* _cbl_nonnull,
                            CBLError*) CBLAPI;

/** Returns the configuration of an existing replicator. */
const CBLReplicatorConfiguration* cbl_repl_config(CBLReplicator* _cbl_nonnull) CBLAPI;

/** Instructs the replicator to ignore existing checkpoints the next time it runs. */
void cbl_repl_resetCheckpoint(CBLReplicator* _cbl_nonnull) CBLAPI;

/** Starts a replicator, asynchronously. Does nothing if it's already started. */
void cbl_repl_start(CBLReplicator* _cbl_nonnull) CBLAPI;

/** Stops a running replicator. Does nothing if it's not already started. */
void cbl_repl_stop(CBLReplicator* _cbl_nonnull) CBLAPI;

/** @} */



/** \name  Status and Progress
    @{ */

/** The possible states a replicator can be in during its lifecycle. */
typedef CBL_ENUM(uint8_t, CBLReplicatorActivityLevel) {
    kCBLReplicatorStopped,    ///< The replicator is unstarted, finished, or hit a fatal error.
    kCBLReplicatorOffline,    ///< The replicator is offline, as the remote host is unreachable.
    kCBLReplicatorConnecting, ///< The replicator is connecting to the remote host.
    kCBLReplicatorIdle,       ///< The replicator is inactive, waiting for changes to sync.
    kCBLReplicatorBusy        ///< The replicator is actively transferring data.
};

/** A fractional progress value. The units are undefined; the only meaningful number is the
    (fractional) result of `completed` ÷ `total`, which will range from 0.0 to 1.0.
    Before anything happens, both `completed` and `total` will be 0. */
typedef struct {
    uint64_t completed, total;
} CBLReplicatorProgress;

/** A replicator's current status. */
typedef struct {
    CBLReplicatorActivityLevel activity;    ///< Current state
    CBLReplicatorProgress progress;         ///< Approximate fraction complete
    CBLError error;                         ///< Error, if any
} CBLReplicatorStatus;

/** Returns the replicator's current status. */
CBLReplicatorStatus cbl_repl_status(CBLReplicator* _cbl_nonnull) CBLAPI;

/** A callback that notifies you when the replicator's status changes.
    @param context  The value given when the listener was added.
    @param replicator  The replicator.
    @param status  The replicator's status. */
typedef void (*CBLReplicatorListener)(void *context, 
                                      CBLReplicator *replicator _cbl_nonnull,
                                      const CBLReplicatorStatus *status _cbl_nonnull);

/** Adds a listener that will be called when the replicator's status changes. */
CBLListenerToken* cbl_repl_addListener(CBLReplicator* _cbl_nonnull,
                                       CBLReplicatorListener _cbl_nonnull, 
                                       void *context) CBLAPI;

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif
