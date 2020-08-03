# Couchbase Lite replicator API
#
# Copyright (c) 2020 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import CouchbaseLite/[database, document, errors, listener]
import CouchbaseLite/private/cbl

import httpcore
import options
import sets
import uri

{.experimental: "notnil".}


## WARNING: THIS API IS UNIMPLEMENTED SO FAR


#======== CONFIGURATION


type
  EndpointType* = enum
    WithURL,
    WithLocalDB
  Endpoint* = object
    ## Represents the location of a database to replicate with.
    case type*: EndpointType:
      of WithURL: url*: Url ## WebSocket URL ("ws:" or "wss:") of remote database
      of WithLocalDB: db*: Database ## Local database (available only in Enterprise Edition)

  AuthenticatorType* = enum
    None,
    Basic,
    Session,
    Cookie
  Authenticator* = object
    ## Represents authentication credentials for a remote server.
    case type*: AuthenticatorType:
      of None: discard
      of Basic: username*, password*: string
      of Session: sessionID*: string
      of Cookie: name*, value*: string

  ReplicatorType* = enum
    ## Direction of replication: push, pull, or both.
    PushAndPull = 0
    Push
    Pull

  ProxyType* = enum HTTP, HTTPS
  ProxySettings* = object
    ## HTTP client proxy settings for the replicator.
    proxyType*: ProxyType
    hostname*: string
    port*: uint16
    username*: string
    password*: string

  ReplicationFilter* = proc(document: Document; isDeleted: bool): bool
    ## A callback that can decide whether a particular document should be pushed or pulled.
  ConflictResolver* = proc(documentID: string; local,
    remote: Document): Document
    ## Conflict-resolution callback for use in replications. This callback will
    ## be invoked when the replicator finds a newer server-side revision of a
    ## document that also has local changes. The local and remote changes must
    ## be resolved before the document can be pushed to the server.

  ReplicatorConfiguration* = object
    ## The configuration of a replicator
    database*: Database                  ## The database to replicate
    endpoint*: Endpoint                  ## The address of the other database to replicate with
    replicatorType*: ReplicatorType      ## Push, pull or both
    continuous*: bool                    ## Continuous replication?

    authenticator*: Authenticator        ## Authentication credentials, if needed
    proxy*: Option[ProxySettings]        ## HTTP client proxy settings
    headers*: HttpHeaders                ## Extra HTTP headers to add to the WebSocket request
    pinnedServerCertificate*: seq[uint8] ## An X.509 cert to "pin" TLS connections to (PEM or DER)
    trustedRootCertificates*: seq[uint8] ## Set of anchor certs (PEM format)

    channels*: seq[string]               ## Optional set of channels to pull from
    documentIDs*: seq[string]            ## Optional set of document IDs to replicate
    pushFilter*: ReplicationFilter       ## Optional callback to filter which docs are pushed
    pullFilter*: ReplicationFilter       ## Optional callback to validate incoming docs
    conflictResolver*: ConflictResolver  ## Optional conflict-resolver callback


#======== LIFECYCLE


type
  ReplicatorObj {.requiresInit.} = object
    handle: CBLReplicator not nil
  Replicator* = ref ReplicatorObj not nil
    ## A background task that syncs a Database with a remote server or peer.

proc `=destroy`(r: var ReplicatorObj) = release(r.handle)
proc `=`(dst: var ReplicatorObj; src: ReplicatorObj) {.error.}


proc newReplicator*(configuration: ReplicatorConfiguration): Replicator =
  ## Creates a replicator with the given configuration.
  throw ErrorCode.Unimplemented

proc configuration*(repl: Replicator): ReplicatorConfiguration =
  ## Returns the configuration of an existing replicator.
  throw ErrorCode.Unimplemented

proc resetCheckpoint*(repl: Replicator) =
  ## Instructs the replicator to ignore existing checkpoints the next time it
  ## runs.  This will cause it to scan through all the documents on the remote
  ## database, which takes a lot longer, but it can resolve problems with
  ## missing documents if the client and server have gotten out of sync
  ## somehow.
  throw ErrorCode.Unimplemented

proc start*(repl: Replicator) =
  ## Starts a replicator, asynchronously. Does nothing if it's already started.
  throw ErrorCode.Unimplemented

proc stop*(repl: Replicator) =
  ## Stops a running replicator, asynchronously. Does nothing if it's not
  ## already started.  The replicator will call your ChangeListener with an
  ## activity level of ``Stopped`` after it stops. Until then, consider it
  ## still active.
  throw ErrorCode.Unimplemented

proc setHostReachable*(repl: Replicator; reachable: bool) =
  ## Informs the replicator whether it's considered possible to reach the
  ## remote host with the current network configuration. The default value is
  ## true. This only affects the replicator's behavior while it's in the
  ## Offline state:
  ## * Setting it to false will cancel any pending retry and prevent future
  ##   automatic retries.
  ## * Setting it back to true will initiate an immediate retry.
  throw ErrorCode.Unimplemented

proc setSuspended*(repl: Replicator; suspended: bool) =
  ## Puts the replicator in or out of "suspended" state. The default is false.
  ## * Setting suspended=true causes the replicator to disconnect and enter
  ##   Offline state; it will not attempt to reconnect while it's suspended.
  ## * Setting suspended=false causes the replicator to attempt to reconnect,
  ##   _if_ it was connected when suspended, and is still in Offline state.
  throw ErrorCode.Unimplemented


#======= STATUS AND PROGRESS


type
  ActivityLevel* = enum
    ## The possible states a replicator can be in during its lifecycle.
    Stopped,    ## unstarted, finished, or hit a fatal error.
    Offline,    ## offline, as the remote host is unreachable.
    Connecting, ## connecting to the remote host.
    Idle,       ## inactive, waiting for changes to sync.
    Busy        ## actively transferring data.

  ReplicatorProgress* = object
    ## The current progress status of a Replicator. The `fraction_complete`
    ## ranges from 0.0 to 1.0 as replication progresses. The value is very
    ## approximate and may bounce around during replication; making it more
    ## accurate would require slowing down the replicator and incurring more
    ## load on the server. It's fine to use in a progress bar, though.
    fractionComplete*: float
    documentCount*: uint64

  ReplicatorStatus* = object
    ## A replicator's current status.
    activity*: ActivityLevel
    progress*: ReplicatorProgress
    error*: Error

proc status*(repl: Replicator): ReplicatorStatus =
  ## Returns the replicator's current status.
  throw ErrorCode.Unimplemented

proc pendingDocIDs*(repl: Replicator): HashSet[string] =
  ## Indicates which documents have local changes that have not yet been pushed
  ## to the server by this replicator. This is of course a snapshot, that will
  ## go out of date as the replicator makes progress and/or documents are saved
  ## locally.
  throw ErrorCode.Unimplemented

proc isDocumentPending*(repl: Replicator; docID: string): bool =
  ## Indicates whether the document with the given ID has local changes that
  ## have not yet been pushed to the server by this replicator.
  ##
  ## This is equivalent to, but faster than, calling ``pendingDocumentIDs`` and
  ## checking whether the result contains ``docID``. See that function's
  ## documentation for details.
  throw ErrorCode.Unimplemented

type
  DocumentFlag = enum deleted, accessRemoved
  Direction = enum pulled, pushed

  ReplicatedDocument* = object
    ## Information about a document that's been pushed or pulled.
    id: string
    flags: set[DocumentFlag]
    error: Error

  ChangeListener* = proc(replicator: Replicator; status: ReplicatorStatus)
    ## A callback that notifies you when the replicator's status changes.
  ReplicatedDocumentListener* = proc(replicator: Replicator;
                                     direction: Direction;
                                     documents: seq[ReplicatedDocument])
    ## A callback that notifies you when documents are replicated.

proc addChangeListener*(repl: Replicator;
    listener: ChangeListener): ListenerToken =
  ## Adds a listener that will be called when the replicator's status changes.
  throw ErrorCode.Unimplemented

proc addDocumentListener*(repl: Replicator): ListenerToken =
  ## Adds a listener that will be called when documents are replicated.
  throw ErrorCode.Unimplemented
