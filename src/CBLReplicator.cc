//
// CBLReplicator.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "CBLReplicator.h"
#include "CBLReplicatorConfig.hh"
#include "CBLDocument_Internal.hh"
#include "Internal.hh"
#include "c4.hh"
#include "c4Replicator.h"
#include "c4Private.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <mutex>

using namespace std;
using namespace fleece;
using namespace cbl_internal;


static inline const CBLReplicatorStatus& external(const C4ReplicatorStatus &status) {
    return (const CBLReplicatorStatus&)status;
}


class CBLReplicator : public CBLRefCounted {
public:
    CBLReplicator(const CBLReplicatorConfiguration *conf _cbl_nonnull)
    :_conf(*conf)
    { }


    const ReplicatorConfiguration* configuration() const        {return &_conf;}
    bool validate(CBLError *err) const                          {return _conf.validate(err);}


    void start() {
        lock_guard<mutex> lock(_mutex);
        _start();
    }


    void stop() {
        lock_guard<mutex> lock(_mutex);
        _stop();
    }


    void resetCheckpoint() {
        lock_guard<mutex> lock(_mutex);
        if (!_c4repl)
            _resetCheckpoint = true;
    }


    CBLReplicatorStatus status() {
        lock_guard<mutex> lock(_mutex);
        if (!_c4repl)
            return {kCBLReplicatorStopped, {0, 0}, {}};
        return external(c4repl_getStatus(_c4repl));
    }


    void setListener(CBLReplicatorListener listener, void *context) {
        lock_guard<mutex> lock(_mutex);
        _listener = listener;
        _listenerContext = context;
    }

private:

    void _start() {
        if (_c4repl)
            return;

        // Set up the LiteCore replicator parameters:
        C4ReplicatorParameters params = { };
        auto type = _conf.continuous ? kC4Continuous : kC4OneShot;
        if (_conf.replicatorType != kCBLReplicatorTypePull)
            params.push = type;
        if (_conf.replicatorType != kCBLReplicatorTypePush)
            params.pull = type;
        params.callbackContext = this;
        params.onStatusChanged = [](C4Replicator* c4repl, C4ReplicatorStatus status, void *ctx) {
            ((CBLReplicator*)ctx)->_statusChanged(c4repl, status);
        };

        if (_conf.pushFilter) {
            params.pushFilter = [](C4String docID,
                                   C4RevisionFlags flags,
                                   FLDict body,
                                   void* ctx)
            {
                return ((CBLReplicator*)ctx)->_filter(docID, flags, body, true);
            };
        }
        if (_conf.pullFilter) {
            params.validationFunc = [](C4String docID,
                                       C4RevisionFlags flags,
                                       FLDict body,
                                       void* ctx)
            {
                return ((CBLReplicator*)ctx)->_filter(docID, flags, body, false);
            };
        }

        alloc_slice properties;
        {
            Encoder enc;
            enc.beginDict();
            _conf.writeOptions(enc);
            if (_resetCheckpoint) {
                enc[slice(kC4ReplicatorResetCheckpoint)] = true;
                _resetCheckpoint = false;
            }
            enc.endDict();
            properties = enc.finish();
        }
        params.optionsDictFleece = properties;

        // Create/start the LiteCore replicator:
        CBLError error;
        _c4repl = c4repl_new(internal(_conf.database),
                             _conf.endpoint->remoteAddress(),
                             _conf.endpoint->remoteDatabaseName(),
                             internal(_conf.endpoint->otherLocalDB()),
                             params,
                             internal(&error));
        if (!_c4repl)
            throw error;
        _stopping = false;
        retain(this);
    }


    void _stop() {
        if (!_c4repl || _stopping)
            return;

        _stopping = true;
        c4repl_stop(_c4repl);
    }


    void _statusChanged(C4Replicator* c4repl, C4ReplicatorStatus status) {
        unique_lock<mutex> lock(_mutex);
        if (c4repl != _c4repl)
            return;

        if (_listener) {
            lock.unlock();
            _listener(_listenerContext, this, &external(status));
            if (status.level == kC4Stopped)
                lock.lock();
        }

        if (status.level == kC4Stopped) {
            c4repl_free(_c4repl);
            _c4repl = nullptr;
            _stopping = false;
            release(this);
        }
    }


    bool _filter(slice docID, C4RevisionFlags flags, Dict body, bool pushing) {
        Retained<CBLDocument> doc = new CBLDocument(_conf.database, string(docID), flags, body);
        CBLReplicationFilter filter = pushing ? _conf.pushFilter : _conf.pullFilter;
        return filter(_conf.filterContext, doc, (flags & kRevDeleted) != 0);
    }


    ReplicatorConfiguration const _conf;
    Retained<CBLDatabase> const _otherLocalDB;
    std::mutex _mutex;
    c4::ref<C4Replicator> _c4repl;
    CBLReplicatorListener _listener {nullptr};
    void* _listenerContext {nullptr};
    bool _resetCheckpoint {false};
    bool _stopping {false};
};


#pragma mark - C API:


CBLEndpoint* cblendpoint_newWithURL(const char *url _cbl_nonnull) CBLAPI {
    return new CBLURLEndpoint(url);
}

void cblendpoint_free(CBLEndpoint *endpoint) CBLAPI {
    delete endpoint;
}

CBLAuthenticator* cblauth_newBasic(const char *username, const char *password) CBLAPI {
    return new BasicAuthenticator(username, password);
}

void cblauth_free(CBLAuthenticator *auth) CBLAPI {
    delete auth;
}

CBLReplicator* cbl_repl_new(const CBLReplicatorConfiguration* conf, CBLError *outError) CBLAPI {
    return validated(new CBLReplicator(conf), outError);
}

const CBLReplicatorConfiguration* cbl_repl_config(CBLReplicator* repl) CBLAPI {
    return repl->configuration();
}

CBLReplicatorStatus cbl_repl_status(CBLReplicator* repl) CBLAPI {
    return repl->status();
}

void cbl_repl_start(CBLReplicator* repl) CBLAPI            {repl->start();}
void cbl_repl_stop(CBLReplicator* repl) CBLAPI             {repl->stop();}
void cbl_repl_resetCheckpoint(CBLReplicator* repl) CBLAPI  {repl->resetCheckpoint();}
