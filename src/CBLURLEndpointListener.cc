//
// CBLURLEndpointListener.cc
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
#include "c4Listener.h"

using namespace fleece;


struct CBLURLEndpointListener : public CBLRefCounted {
public:
    CBLURLEndpointListener(CBLURLEndpointListenerConfiguration* config _cbl_nonnull) {
        _db                 = config->database;
        _c4config.port      = config->port;
        _c4config.networkInterface = config->networkInterface;
        _c4config.apis      = kC4SyncAPI;
        _c4config.allowPush = !config->readOnly;
        // TODO: Fill in tlsIdentity, authenticator
    }

    ~CBLURLEndpointListener() {
        stop();
    }

    bool start(CBLError *outError) {
        if (_listener)
            return true;
        _listener = c4listener_start(&_c4config, internal(outError));
        if (!_listener)
            return false;

        C4Database* c4db = _db->_getC4Database();
        if (!c4listener_shareDB(_listener, nullslice, c4db, internal(outError))) {
            stop();
            return false;
        }
        return true;
    }

    void stop() {
        if (_listener) {
            c4listener_free(_listener);
            _listener = nullptr;
        }
    }

    uint16_t port() const {
        return _listener ? c4listener_getPort(_listener) : 0;
    }

    FLMutableArray URLs() {
        return c4listener_getURLs(_listener, _db->_getC4Database(), kC4SyncAPI, nullptr);
    }

    CBLConnectionStatus status() {
        unsigned count = 0, activeCount = 0;
        c4listener_getConnectionStatus(_listener, &count, &activeCount);
        return {count, activeCount};  //TODO: Get active connection count & return it
    }

private:
    CBLDatabase*        _db;
    C4ListenerConfig    _c4config = {};
    C4Listener*         _listener;
};


#pragma mark - PUBLIC API:


CBLURLEndpointListener* CBLURLEndpointListener_New(CBLURLEndpointListenerConfiguration* config) noexcept {
    return make_nothrow<CBLURLEndpointListener>(nullptr, config).detach();

}

bool CBLURLEndpointListener_Start(CBLURLEndpointListener* listener, CBLError *outError) noexcept {
    return listener->start(outError);
}

void CBLURLEndpointListener_Stop(CBLURLEndpointListener* listener) noexcept {
    listener->stop();
}

uint16_t CBLURLEndpointListener_GetPort(CBLURLEndpointListener* listener) noexcept {
    return listener->port();
}

FLMutableArray CBLURLEndpointListener_GetURLs(CBLURLEndpointListener* listener _cbl_nonnull) noexcept {
    return listener->URLs();
}

CBLConnectionStatus CBLURLEndpointListener_GetStatus(CBLURLEndpointListener* listener) noexcept {
    return listener->status();
}

#endif
