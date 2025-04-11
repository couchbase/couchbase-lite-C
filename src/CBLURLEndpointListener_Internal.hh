//
//  CBLURLEndpointListener_Internal.hh
//
// Copyright © 2025 Couchbase. All rights reserved.
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

#include "CBLURLEndpointListener.h"

#include "Internal.hh"
#include "Base64.hh"
#include "CBLCollection_Internal.hh"
#include "CBLDatabase_Internal.hh"
#include "CBLTLSIdentity_Internal.hh"
#include "c4Listener.hh"
#include <mutex>
using namespace fleece;

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLListenerAuthenticator {
    union {
        CBLListenerPasswordAuthCallback pswCallback;
        CBLListenerCertAuthCallback     certCallback;
        void*                           callback;
    };
    
    bool isCert{false};             // Whether the authenticator is a CertAuth
    Retained<CBLCert> rootCerts;    // For CertAuth created with root certs

    CBLListenerAuthenticator(CBLListenerPasswordAuthCallback callback)
    : pswCallback(callback)
    , isCert(false)
    {}

    CBLListenerAuthenticator(CBLListenerCertAuthCallback callback)
    : certCallback(callback)
    , isCert(true)
    {}
    
    CBLListenerAuthenticator(CBLCert* cert)
    : rootCerts(cert)
    , isCert(true)
    {}

    CBLListenerAuthenticator(const CBLListenerAuthenticator& src)
    : callback(src.callback)
    , rootCerts(src.rootCerts)
    , isCert(src.isCert)
    {}
};

struct CBLURLEndpointListener final : public CBLRefCounted {
public:
    CBLURLEndpointListener(const CBLURLEndpointListenerConfiguration &conf)
    : _conf(conf)
    {
        if (conf.collectionCount == 0) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "No collections in CBLURLEndpointListenerConfiguration");
        }
        if (_conf.authenticator && _conf.authenticator->isCert) {
            if (_conf.disableTLS)
                C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "TLS must be enabled to use the cert authenticator");
        }
        if (_conf.authenticator) _conf.authenticator = new CBLListenerAuthenticator(*_conf.authenticator);
        if (_conf.tlsIdentity) CBLTLSIdentity_Retain(_conf.tlsIdentity);
    }

    ~CBLURLEndpointListener() {
        if (_conf.authenticator) delete _conf.authenticator;
        if (_conf.tlsIdentity)   CBLTLSIdentity_Release(_conf.tlsIdentity);
    }

    const CBLURLEndpointListenerConfiguration* configuration() const { return &_conf; }

    uint16_t port() const {
        if (_port == 0 && _c4listener) {
            _port = _c4listener->port();
        }
        return _port;
    }

    fleece::MutableArray getUrls() const {
        if (!_c4listener) return {};

        CBLDatabase* cblDb = _conf.collections[0]->database();
        fleece::MutableArray urls = fleece::MutableArray::newArray();
        cblDb->c4db()->useLocked([&](C4Database* db) {
            for ( const std::string& url : _c4listener->URLs(db, kC4SyncAPI) )
                urls.append(url);
        });
        return urls;
    }

    CBLConnectionStatus getConnectionStatus() const {
        if (_c4listener) {
            auto [total, active] = _c4listener->connectionStatus();
            return {total, active};
        } else {
            return {0, 0};
        }
    }

    void start();
    void stop();

protected:
    std::string dumpConfig();
    CBLTLSIdentity* _cbl_nullable effectiveTLSIdentity(bool persistent);
    Retained<CBLTLSIdentity>      anonymousTLSIdentity(bool persistent);
    alloc_slice                   labelForAnonymousTLSIdentity();

private:
    static std::mutex                   _mutex;
    CBLURLEndpointListenerConfiguration _conf;
    mutable uint16_t                    _port{0};
    C4Listener* _cbl_nullable           _c4listener{nullptr};
    Retained<CBLTLSIdentity>            _effectiveTLSIdentity;
};

CBL_ASSUME_NONNULL_END

#endif
