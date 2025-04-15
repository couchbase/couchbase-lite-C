//
//  CBLURLEndpointListener.cc
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

#include "CBLURLEndpointListener_Internal.hh"

#ifdef COUCHBASE_ENTERPRISE

std::mutex CBLURLEndpointListener::_mutex;
constexpr CBLTimestamp kCBLAnonymousIdentityMinValidTimeAllowed = 86400; /* 24 Hours */

void CBLURLEndpointListener::start() {
    std::scoped_lock lock(_mutex);

    if (_c4listener) return;

    Assert(_conf.collectionCount > 0);

    C4ListenerConfig c4config {
        _conf.port,
        _conf.networkInterface,
        kC4SyncAPI
    };
    c4config.allowPush = true;
    c4config.allowPull = !_conf.readOnly;
    c4config.enableDeltaSync = _conf.enableDeltaSync;

    C4TLSConfig tls = {};

    if (!_conf.disableTLS) {
        constexpr bool persistent =
#if !defined(__linux__) && !defined(__ANDROID__)
        true;
#else
        false;
#endif
        CBLTLSIdentity* identity = effectiveTLSIdentity(persistent);
        if (!identity) {
            CBL_Log(kCBLLogDomainListener, kCBLLogWarning, "Cannot determine TLSIdentity when TLS is enabled. %s", dumpConfig().c_str());
            C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, "Cannot determine TLSIdentity when TLS is enabled");
        }
        tls.certificate = identity->certificates()->c4Cert();
        if (identity->privateKey()) {
            tls.privateKeyRepresentation = kC4PrivateKeyFromKey;
            tls.key = identity->privateKey()->c4KeyPair();
        } else {
            tls.privateKeyRepresentation = kC4PrivateKeyFromCert;
        }
        tls.requireClientCerts = false;
        c4config.tlsConfig = &tls;
    }

    if (_conf.authenticator) {
        if (_conf.authenticator->isCert) {
            // certificate
            // Pre-condition: !_conf.disableTLS
            tls.requireClientCerts = true;
            if (_conf.authenticator->certCallback) {
                tls.tlsCallbackContext = this;
                tls.certAuthCallback = [](C4Listener *listener, C4Slice clientCertData, void *context) -> bool {
                    auto me = reinterpret_cast<CBLURLEndpointListener*>(context);
                    return me->_conf.authenticator->certCallback(me->_conf.context, clientCertData);
                };
            } else {
                assert(_conf.authenticator->rootCerts);
                tls.rootClientCerts = _conf.authenticator->rootCerts->c4Cert();
            }
        } else {
            // user/password
            c4config.httpAuthCallback = [](C4Listener* listener, C4Slice authHeader, void* context) -> bool {
                slice header{authHeader};
                auto space = header.findByte(' ');
                if (!space) return false;

                else header = header.from(space + 1);
                while ( !header.empty() && header[0] == ' ' )
                    header = header.from(1);
                alloc_slice decoded = fleece::base64::decode(header);
                auto colon = decoded.findByte(':');
                if (!colon) return false;

                slice usr = decoded.upTo(colon);
                slice pwd = decoded.from(colon + 1);

                auto me = reinterpret_cast<CBLURLEndpointListener*>(context);
                Assert(me->_c4listener == listener);
                return me->_conf.authenticator->pswCallback(me->_conf.context, usr, pwd);
            };
            c4config.callbackContext = this;
        }
    }

    CBLDatabase* db = _conf.collections[0]->database();
    bool succ = true;
    
    std::unique_ptr<C4Listener> c4listener{new C4Listener(c4config)};
    auto ret = [&](bool succ) {
        if (succ) {
            db->registerService(this, [this] { stop(); });
            _c4listener = c4listener.release();
        }
    };
    
    db->c4db()->useLocked([&](C4Database* db) {
        slice dbname = db->getName();
        if ( (succ = c4listener->shareDB(dbname, db)) ) {
            for (unsigned i = 0; i < _conf.collectionCount; ++i) {
                _conf.collections[i]->useLocked([&](C4Collection* coll) {
                    succ = c4listener->shareCollection(dbname, coll);
                });
                if (!succ) break;
            }
        }
    });

    ret(succ);
}

void CBLURLEndpointListener::stop() {
    std::scoped_lock lock(_mutex);
    if (_c4listener) {
        CBLDatabase* db = _conf.collections[0]->database();
        db->unregisterService(this);
        
        delete _c4listener;
        _c4listener = nullptr;
    }
}

std::string CBLURLEndpointListener::dumpConfig() {
    return {};
}

CBLTLSIdentity* CBLURLEndpointListener::effectiveTLSIdentity(bool persistent) {
    if (_conf.disableTLS)
        return nullptr;

    if (!_effectiveTLSIdentity)
        _effectiveTLSIdentity = _conf.tlsIdentity ? _conf.tlsIdentity : anonymousTLSIdentity(persistent).get();

    return _effectiveTLSIdentity;
}

Retained<CBLTLSIdentity> CBLURLEndpointListener::anonymousTLSIdentity(bool persistent) {
    Retained<CBLTLSIdentity> identity;
    alloc_slice              label;
    Retained<CBLKeyPair>     keypair;

    if (persistent) {
#if !defined(__linux__) && !defined(__ANDROID__)
        label = labelForAnonymousTLSIdentity();
        if (!label) return nullptr;

        identity = CBLTLSIdentity::IdentityWithLabel(label);
        if (identity) {
            CBL_Log(kCBLLogDomainListener, kCBLLogVerbose, "Found anonymous identity by label = '%.*s'", (int)label.size, (char*)label.buf);

            CBLTimestamp expire = identity->expiration();
            if (expire/1000 > kCBLAnonymousIdentityMinValidTimeAllowed) {
                return identity;
            }

            CBL_Log(kCBLLogDomainListener, kCBLLogVerbose, "Delete anonymous identity of label = '%.*s' (expiration = %ld)", (int)label.size, (char*)label.buf, (long)(expire/1000));

            CBLTLSIdentity::DeleteIdentityWithLabel(label);
        }
        
#else // #if defined(__linux__) || defined(__ANDROID__)
        C4Error::raise(LiteCoreDomain, kC4ErrorUnimplemented, "No persistent key support");
#endif
    }

    fleece::MutableDict attrs = fleece::MutableDict::newDict();
    attrs[kCBLCertAttrKeyCommonName] = "CBLAnonymousCertificate";
    return CBLTLSIdentity::CreateIdentity(kCBLKeyUsagesServerAuth, attrs, 0, label);
}

alloc_slice CBLURLEndpointListener::labelForAnonymousTLSIdentity() {
    alloc_slice uuid = CBLDatabase_PublicUUID(_conf.collections[0]->database());
    return alloc_slice{uuid.hexString()};
}

#endif //#ifdef COUCHBASE_ENTERPRISE
