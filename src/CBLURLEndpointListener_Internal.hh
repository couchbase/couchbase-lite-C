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
#include "Defer.hh"
#include "c4Listener.hh"

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLListenerAuthenticator {
    
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

    FLMutableArray _cbl_nullable getUrls() const {
        if (!_c4listener) return (FLMutableArray) nullptr;

        CBLDatabase* cblDb = CBLCollection_Database(_conf.collections[0]);
        auto urls = fleece::MutableArray::newArray();
        cblDb->c4db()->useLocked([&](C4Database* db) {
            for ( const std::string& url : _c4listener->URLs(db, kC4SyncAPI) )
                urls.append(url);
        });
        return (FLMutableArray)FLValue_Retain(urls);
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

        bool succ = true;
        slice dbname;
        DEFER {
            if (!succ) {
                for (C4Collection* coll: _collections) {
                    // These collections have been successfully shared.
                    (void)_c4listener->unshareCollection(dbname, coll);
                }
                (void)_c4listener->unshareDB(_db);
                delete _c4listener;
                _c4listener = nullptr;
                _collections.clear();
                _db = nullptr;
            }
        };

        Assert(_conf.collectionCount > 0);

        C4ListenerConfig c4config {
            _conf.port,
            _conf.networkInterface,
            kC4SyncAPI
        };
        c4config.allowPush = true;
        c4config.allowPull = !_conf.readOnly;
        c4config.enableDeltaSync = _conf.enableDeltaSync;

        _c4listener = new C4Listener(c4config);

        CBLDatabase* cblDb = CBLCollection_Database(_conf.collections[0]);
        cblDb->c4db()->useLocked([&](C4Database* db) {
            dbname = db->getName();
            if (_c4listener->shareDB(dbname, db)) {
                _db = db;
            }
            if (_db) {
                for (unsigned i = 0; i < _conf.collectionCount; ++i) {
                    _conf.collections[i]->useLocked([&](C4Collection* coll) {
                        if (_c4listener->shareCollection(dbname, coll)) {
                            _collections.push_back(coll);
                        }
                    });
                    if (_collections.size() < i + 1) break;
                }
            }
        });
        return (succ = _collections.size() == _conf.collectionCount);
    }

    void stop() {
        if (!_c4listener) return;

        auto dbname = _db->getName();
        for (C4Collection* coll: _collections) {
            (void)_c4listener->unshareCollection(dbname, coll);
        }
        (void)_c4listener->unshareDB(_db);

        c4listener_free(_c4listener);
        _c4listener = nullptr;
    }

private:
    const CBLURLEndpointListenerConfiguration _conf;
    mutable uint16_t                          _port{0};
    C4Listener* _cbl_nullable                 _c4listener{nullptr};
    C4Database* _cbl_nullable                 _db;
    std::vector<C4Collection*>                _collections;
};

CBL_ASSUME_NONNULL_END

#endif
