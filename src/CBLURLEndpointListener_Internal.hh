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
#include "CBLCollection_Internal.hh"
#include "CBLDatabase_Internal.hh"
#include "c4.h"

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLListenerAuthenticator {
    
};

struct CBLURLEndpointListener final : public CBLRefCounted {
public:
    CBLURLEndpointListener(const CBLURLEndpointListenerConfiguration &conf)
    : _conf(conf)
    { }

    const CBLURLEndpointListenerConfiguration* configuration() const { return &_conf; }

    uint16_t port() const {
        if (_port == 0 && _c4listener) {
            _port = c4listener_getPort(_c4listener);
        }
        return _port;
    }

    FLMutableArray _cbl_nullable getUrls() const {
        FLMutableArray ret = (FLMutableArray) nullptr;
        if (_c4listener) {
            CBLDatabase* cblDb = CBLCollection_Database(_conf.collections[0]);
            cblDb->c4db()->useLocked([&](C4Database* db) {
                ret = c4listener_getURLs(_c4listener, db, kC4SyncAPI, nullptr);
            });
        }
        return ret;
    }

    CBLConnectionStatus getConnectionStatus() const {
        unsigned int total = 0, active = 0;
        if (_c4listener) c4listener_getConnectionStatus(_c4listener, &total, &active);
        return {total, active};
    }

    bool start(CBLError* _cbl_nullable outError) {
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

        _c4listener = c4listener_start(&c4config, internal(outError));
        if (!_c4listener)
            return false;

        slice dbname;
        CBLDatabase* cblDb = CBLCollection_Database(_conf.collections[0]);
        cblDb->c4db()->useLocked([&](C4Database* db) {
            dbname = db->getName();
            if (c4listener_shareDB(_c4listener, dbname, db, internal(outError))) {
                _db = db;
            }
            if (_db) {
                for (unsigned i = 0; i < _conf.collectionCount; ++i) {
                    C4Collection* coll = db->getCollection(_conf.collections[i]->spec());
                    if (!coll) {
                        if (outError) {
                            outError->domain = kCBLDomain;
                            outError->code = kCBLErrorInvalidParameter;
                        }
                        break;
                    }
                    if (c4listener_shareCollection(_c4listener, dbname, coll, internal(outError))) {
                        _collections.push_back(coll);
                    }
                }
            }
        });
        if (_collections.size() == _conf.collectionCount) {
            return true;
        }

        // Otherwise unwind
        for (C4Collection* coll: _collections) {
            // These collections have been successfully shared.
            (void)c4listener_unshareCollection(_c4listener, dbname, coll, nullptr);
        }
        (void)c4listener_unshareDB(_c4listener, _db, nullptr);

        c4listener_free(_c4listener);
        _c4listener = nullptr;
        return false;
    }

    void stop() {
        if (!_c4listener) return;

        auto dbname = _db->getName();
        for (C4Collection* coll: _collections) {
            (void)c4listener_unshareCollection(_c4listener, dbname, coll, nullptr);
        }
        (void)c4listener_unshareDB(_c4listener, _db, nullptr);

        c4listener_free(_c4listener);
        _c4listener = nullptr;
    }

private:
    const CBLURLEndpointListenerConfiguration _conf;
    mutable uint16_t                          _port{0};
    C4Listener* _cbl_nullable                 _c4listener{nullptr};
    C4Database*                               _db;
    std::vector<C4Collection*>                _collections;
};

CBL_ASSUME_NONNULL_END

#endif
