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

#include "Internal.hh"
#include "Base64.hh"
#include "CBLCollection_Internal.hh"
#include "CBLDatabase_Internal.hh"
#include "c4Listener.hh"

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLListenerAuthenticator {
    union {
        CBLListenerPasswordAuthCallback pswCallback;
        CBLListenerCertAuthCallback     certCallback;
    };
    bool withCert{false};
};

struct CBLURLEndpointListener final : public CBLRefCounted {
public:
    CBLURLEndpointListener(const CBLURLEndpointListenerConfiguration &conf)
    : _conf(conf)
    {
        if (conf.collectionCount == 0) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "No collections in CBLURLEndpointListenerConfiguration");
        }
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

    bool start() {
        if (_c4listener) return true;

        Assert(_conf.collectionCount > 0);

        C4ListenerConfig c4config {
            _conf.port,
            _conf.networkInterface,
            kC4SyncAPI
        };
        c4config.allowPush = true;
        c4config.allowPull = !_conf.readOnly;
        c4config.enableDeltaSync = _conf.enableDeltaSync;
        if (_conf.authenticator) {
            if (!_conf.authenticator->withCert) {
                // user/password
                c4config.httpAuthCallback = [](C4Listener* listener, C4Slice authHeader, void* context) {
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
            } else {
                // certificate
            }
        }

        std::unique_ptr<C4Listener> c4listener{new C4Listener(c4config)};
        auto ret = [&](bool succ) -> bool {
            if (succ) {
                _c4listener = c4listener.release();
            }
            return succ;
        };

        CBLDatabase* cblDb = _conf.collections[0]->database();
        bool succ = true;
        cblDb->c4db()->useLocked([&](C4Database* db) {
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

        return ret(succ);
    }

    void stop() {
        if (_c4listener) {
            delete _c4listener;
            _c4listener = nullptr;
        }
    }

private:
    const CBLURLEndpointListenerConfiguration _conf;
    mutable uint16_t                          _port{0};
    C4Listener* _cbl_nullable                 _c4listener{nullptr};
};

CBL_ASSUME_NONNULL_END

#endif
