//
// CBLURLEndpointListener.h
//
// Copyright (c) 2020 Couchbase, Inc All rights reserved.
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
#include "CBLCertificate.h"
#include "fleece/Fleece.h"

#ifndef COUCHBASE_ENTERPRISE
#error This API is part of Couchbase Lite Enterprise Edition only.
#endif

CBL_CAPI_BEGIN

/** \defgroup replication   Replication
    @{ */

/** \name  Replication Listener/Server
    @{ */

    /** HTTP Basic authentication callback.
        @param context  The client-supplied value from the `CBLURLEndpointListenerConfiguration`.
        @param username  The user name provided by the client.
        @param password  The password provided by the client.
        @return  True to allow the connection, false to reject it. */
    typedef bool (*CBLPasswordAuthenticator)(void* context,
                                             FLString username,
                                             FLString password);

    /** TLS client certificate authentication callback.
        @param context  The client-supplied value from the `CBLURLEndpointListenerConfiguration`.
        @param certificate  The certificate presented by the client. This may be a certificate
                            chain; check the `nextInChain` property for supporting certificates.
        @return  True to allow the connection, false to reject it. */
    typedef bool (*CBLClientCertAuthenticator)(void* context,
                                               CBLCertificate* certificate);

    
    /** Describes how clients should authenticate to a CBLURLEndpointListener.
        Either one field or the other should be non-NULL; not both. */
    typedef struct CBLListenerAuthenticator {
        /// For HTTP auth, a callback that validates a username/password.
        CBLPasswordAuthenticator _cbl_nullable passwordAuthenticator;

        /// For TLS auth, a callback to validate the client's certificate.
        CBLClientCertAuthenticator _cbl_nullable clientCertAuthenticator;

        /// For TLS auth, an X.509 CA certificate; if given, clients must provide certificates
        /// that are signed by it.
        CBLCertificate* _cbl_nullable clientCertificate;
    } CBLListenerAuthenticator;


    /** Configuration of a P2P connection listener. */
    typedef struct CBLURLEndpointListenerConfiguration {
        /// Database to share
        CBLDatabase* database;

        /// Port to listen on (0 to pick one at random)
        uint16_t port;

        /// Name or address of network interface to listen on (NULL for all interfaces)
        FLString networkInterface;

        /// If true, listener will not use TLS (not recommended!)
        bool disableTLS;

        /// TLS server certificate & private key
        CBLTLSIdentity* _cbl_nullable tlsIdentity;

        /// Authentication for client connections (NULL for no auth)
        CBLListenerAuthenticator* _cbl_nullable authenticator;

        /// If true, the replicator can send/receive partial updates of documents.
        bool enableDeltaSync;

        /// If true, clients are not allowed to push changes to this database.
        bool readOnly;

        /// A client-supplied value passed to the callbacks.
        void* _cbl_nullable context;
    } CBLURLEndpointListenerConfiguration;


    typedef struct CBLConnectionStatus {
        unsigned connectionCount;           ///< Number of TCP connections
        unsigned activeConnectionCount;     ///< Number of connections actively replicating
    } CBLConnectionStatus;


    typedef struct CBLURLEndpointListener CBLURLEndpointListener;


    /** Creates a P2P connection listener. */
    _cbl_warn_unused
    CBLURLEndpointListener* CBLURLEndpointListener_New(CBLURLEndpointListenerConfiguration*) CBLAPI;

    /** Starts a P2P connection listener. */
    bool CBLURLEndpointListener_Start(CBLURLEndpointListener*, CBLError* _cbl_nullable) CBLAPI;

    /** Returns the actual port number being listened on. */
    uint16_t CBLURLEndpointListener_GetPort(CBLURLEndpointListener*) CBLAPI;

    /** Returns the URL(s) at which the listener can be reached. There is one URL for each network
        address, generally in declining order of usefulness.
        \note  Caller is responsible for releasing the returned object.
        @param listener  The listener.
        @return  The URLs as a Fleece array of strings (which must be released). */
    _cbl_warn_unused
    FLMutableArray CBLURLEndpointListener_GetURLs(CBLURLEndpointListener* listener) CBLAPI;

    /** Returns information about how many current connections a P2P connection listener has. */
    CBLConnectionStatus CBLURLEndpointListener_GetStatus(CBLURLEndpointListener*) CBLAPI;

    /** Stops a P2P connection listener. */
    void CBLURLEndpointListener_Stop(CBLURLEndpointListener*) CBLAPI;

    CBL_REFCOUNTED(CBLURLEndpointListener*, URLEndpointListener);

/** @} */
/** @} */

CBL_CAPI_END
