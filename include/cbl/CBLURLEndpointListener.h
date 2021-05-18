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
        Any of the fields may be NULL, but at least one must be set. */
    typedef struct CBLListenerAuthenticator {
        /// For HTTP authentication, a callback that validates a username/password.
        CBLPasswordAuthenticator _cbl_nullable passwordAuthenticator;

        /// For TLS authentication, a callback to validate the client's certificate.
        CBLClientCertAuthenticator _cbl_nullable clientCertAuthenticator;

        /// For TLS authentication, an X.509 CA certificate; if given, clients must provide
        /// certificates that are signed by it.
        CBLCertificate* _cbl_nullable clientCertificate;

        /// A client-supplied value passed to the callbacks.
        void* _cbl_nullable context;
    } CBLListenerAuthenticator;


    /** Configuration of a P2P connection listener.  It's highly advisable to initialize this struct
        to all zeroes, then fill in the fields you want. Only `database` is absolutely required. */
    typedef struct CBLURLEndpointListenerConfiguration {
        /// Local database to share.
        CBLDatabase* database;

        /// TCP port to listen on (0 to pick one at random)
        uint16_t port;

        /// Name or address of network interface to listen on (NULL for all interfaces.)
        /// In most cases you can leave this NULL. But if the device is multi-homed and one of the
        /// networks is more secure than the other, you may want to limit sharing to the secure
        /// network.
        FLString networkInterface;

        /// If true, the listener will not use TLS. **This is not recommended!** Even the minimal
        /// automatic TLS provides encryption. But it can be useful for troubleshooting if you need
        /// to sniff the network traffic.
        bool disableTLS;

        /// TLS server certificate and private key.
        /// If left NULL, and `disableTLS` is not true, an anonymous self-signed server cert
        /// will be created and used. This serves to encrypt traffic, though it doesn't provide
        /// any authentication for clients. For that you do need a real server certificate.
        CBLTLSIdentity* _cbl_nullable tlsIdentity;

        /// Authentication for incoming client connections (NULL for no auth.)
        CBLListenerAuthenticator* _cbl_nullable authenticator;

        /// If true, the replicator can send/receive partial updates of documents.
        /// This reduces network bandwidth but increases CPU usage; it's not usually helpful in a
        /// LAN environment.
        bool enableDeltaSync;

        /// If true, clients are not allowed to push changes to this database, only pull from it.
        bool readOnly;
    } CBLURLEndpointListenerConfiguration;


    /** Returned from \ref CBLURLEndpointListener_GetStatus. */
    typedef struct CBLConnectionStatus {
        unsigned connectionCount;           ///< Number of TCP connections
        unsigned activeConnectionCount;     ///< Number of connections actively replicating
    } CBLConnectionStatus;


    typedef struct CBLURLEndpointListener CBLURLEndpointListener;


    /** Creates a P2P connection listener, without starting it. */
    _cbl_warn_unused
    CBLURLEndpointListener* CBLURLEndpointListener_New(CBLURLEndpointListenerConfiguration*) CBLAPI;

    /** Starts a P2P connection listener. */
    bool CBLURLEndpointListener_Start(CBLURLEndpointListener*, CBLError* _cbl_nullable) CBLAPI;

    /** Returns the actual port number being listened on. */
    uint16_t CBLURLEndpointListener_GetPort(CBLURLEndpointListener*) CBLAPI;

    /** Returns the URL(s) at which the listener can be reached. There is one URL for each active
        network interface, generally given in declining order of usefulness.
        In most cases you can just use the first URL and ignore the rest; however, on a device with
        both WiFi and cellular, a URL will be returned for each one. Telling them apart is
        platform-specific.
        \note  Caller is responsible for releasing the returned object.
        @param listener  The listener.
        @return  The URLs as a Fleece array of strings (which must be released). */
    _cbl_warn_unused
    FLMutableArray CBLURLEndpointListener_GetURLs(CBLURLEndpointListener* listener) CBLAPI;

    /** Returns information about how many current connections a P2P connection listener has. */
    CBLConnectionStatus CBLURLEndpointListener_GetStatus(CBLURLEndpointListener*) CBLAPI;

    /** Stops a P2P connection listener. You may restart it later. */
    void CBLURLEndpointListener_Stop(CBLURLEndpointListener*) CBLAPI;

    CBL_REFCOUNTED(CBLURLEndpointListener*, URLEndpointListener);

/** @} */
/** @} */

CBL_CAPI_END
