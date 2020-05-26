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

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup replication   Replication
    @{ */

/** \name  Replication Listener/Server
    @{ */

    /** Authentication callback. Given a username and password, it should return true to allow the
        connection, false to reject it. */
    typedef bool (*CBLPasswordAuthenticator)(const char *username, const char *password);

    
    /** Describes how clients should authenticate to a CBLURLEndpointListener. */
    typedef struct {
        CBLPasswordAuthenticator    passwordAuthenticator;  ///< Username/password callback for HTTP auth
        CBLCertificate*             certificate;            ///< X.509 certificate for TLS auth
    } CBLListenerAuthenticator;


    /** Configuration of a P2P connection listener. */
    typedef struct {
        CBLDatabase*                database;           ///< Database to share
        uint16_t                    port;               ///< Port to listen on (0 to pick one at random)
        const char*                 networkInterface;   ///< Name or address of network interface to listen on (NULL for all interfaces)
        CBLTLSIdentity*             tlsIdentity;        ///< TLS server certificate & private key
        CBLListenerAuthenticator*   authenticator;      ///< Authentication for client connections (NULL for no auth)
        bool                        readOnly;           ///< True to prevent peers from altering the database
    } CBLURLEndpointListenerConfiguration;


    typedef struct {
        unsigned connectionCount;           ///< Number of TCP connections
        unsigned activeConnectionCount;     ///< Number of connections actively replicating
    } CBLConnectionStatus;

    typedef struct CBLURLEndpointListener CBLURLEndpointListener;

    /** Creates a P2P connection listener. */
    CBLURLEndpointListener* CBLURLEndpointListener_New(CBLURLEndpointListenerConfiguration* _cbl_nonnull);

    /** Starts a P2P connection listener. */
    bool CBLURLEndpointListener_Start(CBLURLEndpointListener* _cbl_nonnull, CBLError*);

    /** Returns the actual port number being listened on. */
    uint16_t CBLURLEndpointListener_GetPort(CBLURLEndpointListener* _cbl_nonnull);

    /** Returns the URL(s) at which the listener can be reached. There is one URL for each network
        address, generally in declining order of usefulness.
        \note  Caller is responsible for releasing the returned object.
        @param listener  The listener.
        @return  The URLs as a Fleece array of strings. */
    FLMutableArray CBLURLEndpointListener_GetURLs(CBLURLEndpointListener* listener _cbl_nonnull);

    /** Returns information about how many current connections a P2P connection listener has. */
    CBLConnectionStatus CBLURLEndpointListener_GetStatus(CBLURLEndpointListener* _cbl_nonnull);

    /** Stops a P2P connection listener. */
    void CBLURLEndpointListener_Stop(CBLURLEndpointListener* _cbl_nonnull);

    CBL_REFCOUNTED(CBLURLEndpointListener*, URLEndpointListener);

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif
