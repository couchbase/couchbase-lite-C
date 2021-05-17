//
// CBLURLEndpointListener.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
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

#ifdef COUCHBASE_ENTERPRISE

#include "CBLURLEndpointListener.h"
#include "Internal.hh"
#include "CBLDatabase_Internal.hh"
#include "CBLCertificate_Internal.hh"
#include "c4Certificate.hh"
#include "c4Listener.hh"
#include "c4Listener.h"
#include "Base64.hh"

using namespace fleece;


struct CBLURLEndpointListener : public CBLRefCounted {
public:
    CBLURLEndpointListener(CBLURLEndpointListenerConfiguration* config) {
        _db                 = config->database;
        _c4config.port      = config->port;
        _c4config.networkInterface = config->networkInterface;
        _c4config.apis      = kC4SyncAPI;
        _c4config.allowPush = true;
        _c4config.allowPull = !config->readOnly;
        _c4config.enableDeltaSync = config->enableDeltaSync;
        _callbackContext    = config->context;

        if (!config->disableTLS) {
            // Create C4TLSConfig:
            _tlsConfig = std::make_unique<C4TLSConfig>();
            memset(_tlsConfig.get(), 0, sizeof(*_tlsConfig));
            _tlsConfig->privateKeyRepresentation = kC4PrivateKeyFromKey;
            _identity = config->tlsIdentity;
            if (!_identity)
                _identity = CBLTLSIdentity::generateAnonymous();
            _tlsConfig->key = _identity->c4KeyPair();
            _tlsConfig->certificate = _identity->c4Cert();
            _c4config.tlsConfig = _tlsConfig.get();
        }

        if (config->authenticator) {
            _passwordAuthenticator = config->authenticator->passwordAuthenticator;
            if (_passwordAuthenticator) {
                // Password auth:
                _c4config.callbackContext = this;
                _c4config.httpAuthCallback = [](C4Listener *listener,
                                                C4Slice authHeader,
                                                void * C4NULLABLE context) -> bool {
                    return ((CBLURLEndpointListener*)context)->httpAuth(authHeader);
                };
            }

            _clientCertAuthenticator = config->authenticator->clientCertAuthenticator;
            if (_clientCertAuthenticator) {
                _c4config.tlsConfig->requireClientCerts = true;
                _c4config.tlsConfig->tlsCallbackContext = this;
                _c4config.tlsConfig->certAuthCallback = [](C4Listener *listener,
                                                           C4Slice clientCertData,
                                                           void * C4NULLABLE context) -> bool {
                    return ((CBLURLEndpointListener*)context)->clientCertAuth(clientCertData);
                };
            }

            if (config->authenticator->clientCertificate) {
                // Client-cert auth:
                assert(_c4config.tlsConfig);    //TODO: Create anonymous identity if this is null
                _clientCert = config->authenticator->clientCertificate->c4Cert();
                _c4config.tlsConfig->requireClientCerts = true;
                _c4config.tlsConfig->rootClientCerts = _clientCert;
            }
        }
    }

    bool start() {
        if (_listener)
            return true;
        _listener.emplace(_c4config);
        if (!_listener->shareDB(nullslice, _db->useLocked().get())) {
            stop();
            return false;
        }
        return true;
    }

    void stop() {
        _listener = nullopt;    // (This destructs the C4Listener, stopping it.)
    }

    uint16_t port() const {
        if (!_listener)
            return 0;
        return _listener->port();
    }

    FLMutableArray URLs() {
        if (!_listener)
            return nullptr;
        return c4listener_getURLs(&*_listener, _db->useLocked().get(), kC4SyncAPI, nullptr);
    }

    CBLConnectionStatus status() {
        if (!_listener)
            return {0, 0};
        auto [count, activeCount] = _listener->connectionStatus();
        return {count, activeCount};
    }

private:
    bool httpAuth(slice authHeader) {
        // `authHeader` must be of the form `"Basic " + base64(username + ":" + password)`.
        if (!authHeader.hasPrefix("Basic "))
            return false;
        alloc_slice credential = fleece::base64::decode(authHeader.from(6));
        auto colon = credential.findByte(':');
        if (!colon)
            return false;
        slice username = credential.upTo(colon), password = credential.from(colon + 1);
        return _passwordAuthenticator(_callbackContext, username, password);
    }

    bool clientCertAuth(slice clientCertData) {
        try {
            auto cert = CBLCertificate::fromData(clientCertData);
            return _clientCertAuthenticator(_callbackContext, cert);
        } catchAndWarn()
    }

    CBLDatabase*                 _db;
    C4ListenerConfig             _c4config = {};
    std::unique_ptr<C4TLSConfig> _tlsConfig;
    Retained<CBLTLSIdentity>     _identity;
    Retained<C4Cert>             _clientCert;
    void*                        _callbackContext = {};
    CBLPasswordAuthenticator     _passwordAuthenticator = nullptr;
    CBLClientCertAuthenticator   _clientCertAuthenticator = nullptr;
    optional<C4Listener>         _listener;
};

#endif
