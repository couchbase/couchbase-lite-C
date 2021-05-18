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

CBL_CAPI_BEGIN

/** \defgroup replication   Replication
    A replicator is a background task that synchronizes changes between a local database and
    another database on a remote server (or on a peer device, or even another local database.)
    @{ */

/** \name  Configuration
    @{ */

/** The name of the HTTP cookie used by Sync Gateway to store session keys. */
CBL_CORE_API extern const FLString kCBLAuthDefaultCookieName;


/** An opaque object representing the location of a database to replicate with. */
typedef struct CBLEndpoint CBLEndpoint;


/** Creates a new endpoint representing a server-based database at the given URL.
    The URL's scheme must be `ws` or `wss`, it must of course have a valid hostname,
    and its path must be the name of the database on that server.
    The port can be omitted; it defaults to 80 for `ws` and 443 for `wss`.
    For example: `wss://example.org/dbname` */
_cbl_warn_unused
CBLEndpoint* CBLEndpoint_NewWithURL(FLString url) CBLAPI;


#ifdef COUCHBASE_ENTERPRISE
/** Creates a new endpoint representing another local database. (Enterprise Edition only.) */
_cbl_warn_unused
CBLEndpoint* CBLEndpoint_NewWithLocalDB(CBLDatabase*) CBLAPI;
#endif


/** Frees a CBLEndpoint object. */
void CBLEndpoint_Free(CBLEndpoint* _cbl_nullable) CBLAPI;


/** An opaque object representing authentication credentials for a remote server. */
typedef struct CBLAuthenticator CBLAuthenticator;


/** Creates an authenticator for HTTP Basic (username/password) auth. */
_cbl_warn_unused
CBLAuthenticator* CBLAuth_NewPassword(FLString username,
                                      FLString password) CBLAPI;


/** Creates an authenticator using a Couchbase Sync Gateway login session identifier,
    and optionally a cookie name (pass NULL for the default.) */
_cbl_warn_unused
CBLAuthenticator* CBLAuth_NewSession(FLString sessionID,
                                     FLString cookieName) CBLAPI;


/** Frees a CBLAuthenticator object. */
void CBLAuth_Free(CBLAuthenticator* _cbl_nullable) CBLAPI;


/** Direction of replication: push, pull, or both. */
typedef CBL_ENUM(uint8_t, CBLReplicatorType) {
    kCBLReplicatorTypePushAndPull = 0,    ///< Bidirectional; both push and pull
    kCBLReplicatorTypePush,               ///< Pushing changes to the target
    kCBLReplicatorTypePull                ///< Pulling changes from the target
};


/** Flags describing a replicated document. */
typedef CBL_OPTIONS(unsigned, CBLDocumentFlags) {
    kCBLDocumentFlagsDeleted        = 1 << 0,   ///< The document has been deleted.
    kCBLDocumentFlagsAccessRemoved  = 1 << 1    ///< Lost access to the document on the server.
};


/** A callback that can decide whether a particular document should be pushed or pulled.
    @warning  This callback will be called on a background thread managed by the replicator.
                It must pay attention to thread-safety. It should not take a long time to return,
                or it will slow down the replicator.
    @param context  The `context` field of the \ref CBLReplicatorConfiguration.
    @param document  The document in question.
    @param flags  Indicates whether the document was deleted or removed.
    @return  True if the document should be replicated, false to skip it. */
typedef bool (*CBLReplicationFilter)(void* _cbl_nullable context,
                                     CBLDocument* document,
                                     CBLDocumentFlags flags);


/** Conflict-resolution callback for use in replications. This callback will be invoked
    when the replicator finds a newer server-side revision of a document that also has local
    changes. The local and remote changes must be resolved before the document can be pushed
    to the server.
    @warning  This callback will be called on a background thread managed by the replicator.
                It must pay attention to thread-safety. However, unlike a filter callback,
                it does not need to return quickly. If it needs to prompt for user input,
                that's OK.
    @param context  The `context` field of the \ref CBLReplicatorConfiguration.
    @param documentID  The ID of the conflicted document.
    @param localDocument  The current revision of the document in the local database,
                or NULL if the local document has been deleted.
    @param remoteDocument  The revision of the document found on the server,
                or NULL if the document has been deleted on the server.
    @return  The resolved document to save locally (and push, if the replicator is pushing.)
        This can be the same as \p localDocument or \p remoteDocument, or you can create
        a mutable copy of either one and modify it appropriately.
        Or return NULL if the resolution is to delete the document. */
typedef const CBLDocument* _cbl_nullable (*CBLConflictResolver)(
                                                  void* _cbl_nullable context,
                                                  FLString documentID,
                                                  const CBLDocument* _cbl_nullable localDocument,
                                                  const CBLDocument* _cbl_nullable remoteDocument);


/** Default conflict resolver. This always returns `localDocument`. */
extern const CBLConflictResolver CBLDefaultConflictResolver;


/** Types of proxy servers, for CBLProxySettings. */
typedef CBL_ENUM(uint8_t, CBLProxyType) {
    kCBLProxyHTTP,                      ///< HTTP proxy; must support 'CONNECT' method
    kCBLProxyHTTPS,                     ///< HTTPS proxy; must support 'CONNECT' method
};


/** Proxy settings for the replicator. If the device is connected to a network that requires a
    proxy to connect to the Internet, you'll need to fill this out (there are platform APIs to get
    these settings) and point to it in the \ref CBLReplicatorConfiguration. */
typedef struct {
    CBLProxyType type;              ///< Type of proxy
    FLString hostname;              ///< Proxy server hostname or IP address
    uint16_t port;                  ///< Proxy server port
    FLString username;              ///< Username for proxy auth (optional)
    FLString password;              ///< Password for proxy auth (optional)
} CBLProxySettings;


/** The configuration of a replicator. It's highly advisable to initialize this struct to all
    zeroes, then fill in the fields you want. Only the first two are absolutely required. */
typedef struct {
    //-- Required properties:

    CBLDatabase* database;              ///< The database to replicate
    CBLEndpoint* endpoint;              ///< The address of the other database to replicate with
    CBLReplicatorType replicatorType;   ///< Push, pull or both
    bool continuous;                    ///< Continuous replication?

    //-- Retry Logic:

    /// Maximum number of attempts to connect (initial connection plus any retries.)
    /// So for example, 1 means no retries if the first attempt fails.
    ///
    /// Specify 0 to get the default value, which is 10 times for a non-continuous replicator and
    /// infinite for a continuous replicator.
    unsigned maxAttempts;

    /// Maximum wait time between retry attempts, in seconds.
    /// The first attempt will be made after only a few seconds. After that, every consecutive
    /// failure to connect doubles the wait; but it will never exceed this amount.
    ///
    /// Specify 0 to get the default value of 300 seconds (5 minutes).
    unsigned maxAttemptWaitTime;

    //-- WebSocket:

    /// The WebSocket "heartbeat" interval in seconds: how often the connection will send a "ping"
    /// message and await a "pong" reply. This checks that the connection is still working, and
    /// also prevents disconnection by middleware that terminates idle sockets.
    ///
    /// Specify 0 to get the default value of 300 seconds (5 minutes).
    unsigned heartbeat;

    //-- HTTP settings:

    /// Points to optional authentication credentials (password or session).
    CBLAuthenticator* _cbl_nullable authenticator;

    /// Points to optional HTTP client-side proxy settings. (This has nothing to do with server-side
    /// reverse proxies. If the client device is on a network that requires a proxy to connect
    /// to hosts outside, this will specify where that proxy is and how to connect to it.)
    const CBLProxySettings* _cbl_nullable proxy;

    /// Optional extra HTTP headers to add to the initial WebSocket connection request.
    /// These are given as a Fleece dictionary whose keys are header names and values are strings.
    FLDict _cbl_nullable headers;

    //-- TLS settings:

    /// An optional X.509 certificate to "pin" TLS connections to, in DER or PEM format.
    /// If this is given, then the TLS connection will accept this, and only this, cert.
    /// This increases security of TLS connections but takes a bit of work to use, since you have
    /// to hardcode the cert into your app, and need to handle the cert expiring or being revoked
    /// in the future.
    FLSlice pinnedServerCertificate;

    /// A set of trusted "anchor" certs, concatenated together in PEM format.
    /// If given, these and only these certificates will be trusted as roots. This overrides the
    /// system list of trusted roots. On some systems this list is not available, so specifying
    /// your own list here (or using \ref pinnedServerCertificate) is required.
    FLSlice trustedRootCertificates;

    //-- Filtering:

    /// Optional set of Sync Gateway channels to pull from, as a Fleece array of strings.
    FLArray _cbl_nullable channels;

    /// Optional set of document IDs to replicate, as a Fleece array of strings.
    /// If given, only these documents will be pushed or pulled.
    FLArray _cbl_nullable documentIDs;

    /// Optional callback, to filter outgoing (pushed) documents.
    /// Given a \ref CBLDocument, it should return true to push it, false to skip it.
    ///
    /// This is more expensive than using `documentIDs`; use it only if you need to filter based
    /// on document contents.
    CBLReplicationFilter _cbl_nullable pushFilter;

    /// Optional callback, to validate incoming (pulled) documents.
    /// Given a \ref CBLDocument, it should return true to add it to the database, false to skip it.
    ///
    /// It's not a good idea to use this for filtering, becuase the replicator still has to go to
    /// the work of downloading the document; use `channels` or `documentIDs` instead.
    /// But it's very valuable for validation, to ensure documents adhere to a schema or aren't
    /// otherwise incorrect, especially in P2P replication.
    CBLReplicationFilter _cbl_nullable pullFilter;

    /// Optional callback to resolve replication conflicts.
    CBLConflictResolver _cbl_nullable conflictResolver;

    /// An arbitrary value that will be passed to the callbacks above.
    void* _cbl_nullable context;

#ifdef COUCHBASE_ENTERPRISE
    //-- Enterprise Edition Only:

    /// This parameter should usually be set to true when making a peer-to-peer connection to
    /// another instance of Couchbase Lite, and false otherwise.
    ///
    /// It changes the behavior of TLS certificate verification such that the replicator will _only_
    /// accept a self-signed certificate. Otherwise, self-signed certificates (other than globally
    /// known roots) are rejected as usual.
    ///
    /// The reason for this is that the TLS server run by a peer will almost always be using a
    /// made-up self-signed cert, since there's usually no central certificate authority creating
    /// server certs for everyone. This has the benefit of encrypting traffic, although of course it
    /// doesn't serve to identify the server.
    ///
    /// That's generally sufficient for a small P2P deployment, where you're making a direct
    /// connection over a LAN and use some mechanism like a Bonjour / DNS-SD service name to locate
    /// the peer before connecting.
    ///
    /// If you're worried about attacks in such an environment, then you'll be provisioning
    /// real server certs for your devices to use in their P2P listeners, and you'll leave this
    /// flag false.
    bool acceptOnlySelfSignedServerCertificate;
#endif
} CBLReplicatorConfiguration;


/** @} */


/** \name  Lifecycle
    @{ */

CBL_REFCOUNTED(CBLReplicator*, Replicator);

/** Creates a replicator with the given configuration. */
_cbl_warn_unused
CBLReplicator* CBLReplicator_New(const CBLReplicatorConfiguration*,
                                 CBLError* _cbl_nullable outError) CBLAPI;

/** Returns the configuration of an existing replicator. */
const CBLReplicatorConfiguration* CBLReplicator_Config(CBLReplicator*) CBLAPI;

/** Starts a replicator, asynchronously. Does nothing if it's already started.
    @param replicator  The replicator instance.
    @param resetCheckpoint  If true, the persistent saved state ("checkpoint") for this replication
                        will be discarded, causing it to re-scan all documents. This significantly
                        increases time and bandwidth (redundant docs are not transferred, but their
                        IDs are) but can resolve unexpected problems with missing documents if one
                        side or the other has gotten out of sync. */
void CBLReplicator_Start(CBLReplicator *replicator,
                         bool resetCheckpoint) CBLAPI;

/** Stops a running replicator, asynchronously. Does nothing if it's not already started.
    The replicator will call your \ref CBLReplicatorChangeListener with an activity level of
    \ref kCBLReplicatorStopped after it stops. Until then, consider it still active. */
void CBLReplicator_Stop(CBLReplicator*) CBLAPI;

/** Informs the replicator whether it's considered possible to reach the remote host with
    the current network configuration. The default value is true. This only affects the
    replicator's behavior while it's in the Offline state:
    * Setting it to false will cancel any pending retry and prevent future automatic retries.
    * Setting it back to true will initiate an immediate retry.*/
void CBLReplicator_SetHostReachable(CBLReplicator*,
                                    bool reachable) CBLAPI;

/** Puts the replicator in or out of "suspended" state. The default is false.
    * Setting suspended=true causes the replicator to disconnect and enter Offline state;
      it will not attempt to reconnect while it's suspended.
    * Setting suspended=false causes the replicator to attempt to reconnect, _if_ it was
      connected when suspended, and is still in Offline state. */
void CBLReplicator_SetSuspended(CBLReplicator* repl, bool suspended) CBLAPI;

/** @} */



/** \name  Status and Progress
    @{
 */

/** The possible states a replicator can be in during its lifecycle. */
typedef CBL_ENUM(uint8_t, CBLReplicatorActivityLevel) {
    kCBLReplicatorStopped,    ///< The replicator is unstarted, finished, or hit a fatal error.
    kCBLReplicatorOffline,    ///< The replicator is offline, as the remote host is unreachable.
    kCBLReplicatorConnecting, ///< The replicator is connecting to the remote host.
    kCBLReplicatorIdle,       ///< The replicator is inactive, waiting for changes to sync.
    kCBLReplicatorBusy        ///< The replicator is actively transferring data.
};

/** A fractional progress value, ranging from 0.0 to 1.0 as replication progresses.
    The value is very approximate and may bounce around during replication; making it more
    accurate would require slowing down the replicator and incurring more load on the server.
    It's fine to use in a progress bar, though. */
typedef struct {
    float fractionComplete;     /// Very-approximate completion, from 0.0 to 1.0
    uint64_t documentCount;     ///< Number of documents transferred so far
} CBLReplicatorProgress;

/** A replicator's current status. */
typedef struct {
    CBLReplicatorActivityLevel activity;    ///< Current state
    CBLReplicatorProgress progress;         ///< Approximate fraction complete
    CBLError error;                         ///< Error, if any
} CBLReplicatorStatus;

/** Returns the replicator's current status. */
CBLReplicatorStatus CBLReplicator_Status(CBLReplicator*) CBLAPI;

/** Indicates which documents have local changes that have not yet been pushed to the server
    by this replicator. This is of course a snapshot, that will go out of date as the replicator
    makes progress and/or documents are saved locally.

    The result is, effectively, a set of document IDs: a dictionary whose keys are the IDs and
    values are `true`.
    If there are no pending documents, the dictionary is empty.
    On error, NULL is returned.

    \note  This function can be called on a stopped or un-started replicator.
    \note  Documents that would never be pushed by this replicator, due to its configuration's
           `pushFilter` or `docIDs`, are ignored.
    \warning  You are responsible for releasing the returned array via \ref FLValue_Release. */
_cbl_warn_unused
FLDict CBLReplicator_PendingDocumentIDs(CBLReplicator*,
                                        CBLError* _cbl_nullable outError) CBLAPI;

/** Indicates whether the document with the given ID has local changes that have not yet been
    pushed to the server by this replicator.

    This is equivalent to, but faster than, calling \ref CBLReplicator_PendingDocumentIDs and
    checking whether the result contains \p docID. See that function's documentation for details.

    \note  A `false` result means the document is not pending, _or_ there was an error.
           To tell the difference, compare the error code to zero. */
bool CBLReplicator_IsDocumentPending(CBLReplicator *repl,
                                     FLString docID,
                                     CBLError* _cbl_nullable outError) CBLAPI;


/** A callback that notifies you when the replicator's status changes.
    @warning  This callback will be called on a background thread managed by the replicator.
                It must pay attention to thread-safety. It should not take a long time to return,
                or it will slow down the replicator.
    @param context  The value given when the listener was added.
    @param replicator  The replicator.
    @param status  The replicator's status. */
typedef void (*CBLReplicatorChangeListener)(void* _cbl_nullable context,
                                            CBLReplicator *replicator,
                                            const CBLReplicatorStatus *status);

/** Adds a listener that will be called when the replicator's status changes. */
_cbl_warn_unused
CBLListenerToken* CBLReplicator_AddChangeListener(CBLReplicator*,
                                                  CBLReplicatorChangeListener,
                                                  void* _cbl_nullable context) CBLAPI;


/** Information about a document that's been pushed or pulled. */
typedef struct {
    FLString ID;                ///< The document ID
    CBLDocumentFlags flags;     ///< Indicates whether the document was deleted or removed
    CBLError error;             ///< If the code is nonzero, the document failed to replicate.
} CBLReplicatedDocument;

/** A callback that notifies you when documents are replicated.
    @warning  This callback will be called on a background thread managed by the replicator.
                It must pay attention to thread-safety. It should not take a long time to return,
                or it will slow down the replicator.
    @param context  The value given when the listener was added.
    @param replicator  The replicator.
    @param isPush  True if the document(s) were pushed, false if pulled.
    @param numDocuments  The number of documents reported by this callback.
    @param documents  An array with information about each document. */
typedef void (*CBLDocumentReplicationListener)(void *context,
                                               CBLReplicator *replicator,
                                               bool isPush,
                                               unsigned numDocuments,
                                               const CBLReplicatedDocument* documents);

/** Adds a listener that will be called when documents are replicated. */
_cbl_warn_unused CBLListenerToken*
CBLReplicator_AddDocumentReplicationListener(CBLReplicator*,
                                             CBLDocumentReplicationListener,
                                             void* _cbl_nullable context) CBLAPI;

/** @} */
/** @} */

CBL_CAPI_END
