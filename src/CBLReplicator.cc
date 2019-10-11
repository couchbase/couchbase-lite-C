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
#include "StringUtil.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <atomic>
#include <mutex>

using namespace std;
using namespace fleece;
using namespace cbl_internal;


#define LOCK(MUTEX)  lock_guard<recursive_mutex> _lock(MUTEX)

extern "C" {
    void C4RegisterBuiltInWebSocket();
}

static CBLReplicatorStatus external(const C4ReplicatorStatus &c4status) {
    return {
        CBLReplicatorActivityLevel(c4status.level),
        {
            c4status.progress.unitsCompleted / max(float(c4status.progress.unitsTotal), 1.0f),
            c4status.progress.documentCount
        },
        external(c4status.error)
    };
}


class CBLReplicator : public CBLRefCounted {
public:
    CBLReplicator(const CBLReplicatorConfiguration *conf _cbl_nonnull)
    :_conf(*conf)
    ,_db(conf->database)
    {
        // One-time initialization of network transport:
        static once_flag once;
        call_once(once, bind(&C4RegisterBuiltInWebSocket));

        // Set up the LiteCore replicator parameters:
        if (!_conf.validate(external(&_status.error)))
            return;
        C4ReplicatorParameters params = { };
        auto type = _conf.continuous ? kC4Continuous : kC4OneShot;
        if (_conf.replicatorType != kCBLReplicatorTypePull)
            params.push = type;
        if (_conf.replicatorType != kCBLReplicatorTypePush)
            params.pull = type;
        params.callbackContext = this;
        params.onStatusChanged = [](C4Replicator* c4repl, C4ReplicatorStatus status, void *ctx) {
            ((CBLReplicator*)ctx)->_statusChanged(status);
        };
        params.onDocumentsEnded = [](C4Replicator* c4repl,
                                     bool pushing,
                                     size_t numDocs,
                                     const C4DocumentEnded* docs[],
                                     void *ctx) {
            ((CBLReplicator*)ctx)->_documentsEnded(pushing, numDocs, docs);
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

        // Encode replicator options dict:
        alloc_slice options = encodeOptions();
        params.optionsDictFleece = options;

        // Create the LiteCore replicator:
        _conf.database->use([&](C4Database *c4db) {
#ifdef COUCHBASE_ENTERPRISE
            if (_conf.endpoint->otherLocalDB()) {
                _c4repl = c4repl_newLocal(c4db,
                                          _conf.endpoint->otherLocalDB(),
                                          params,
                                          &_status.error);
            } else
#endif
            {
                _c4repl = c4repl_new(c4db,
                                     _conf.endpoint->remoteAddress(),
                                     _conf.endpoint->remoteDatabaseName(),
                                     params,
                                     &_status.error);
            }
        });
        if (_c4repl)
            _status = c4repl_getStatus(_c4repl);
    }


    bool validate(CBLError *err) const {
        if (!_c4repl) {
            if (err) *err = external(_status.error);
            return false;
        }
        return true;
    }


    // The rest of the implementation can assume that _c4repl is non-null, and fixed,
    // because CBLReplicator_New will detect a replicator that fails validate() and return NULL.


    const ReplicatorConfiguration* configuration() const    {return &_conf;}
    void setHostReachable(bool reachable)           {c4repl_setHostReachable(_c4repl, reachable);}
    void setSuspended(bool suspended)               {c4repl_setSuspended(_c4repl, suspended);}
    void stop()                                     {c4repl_stop(_c4repl);}


    void resetCheckpoint() {
        LOCK(_mutex);
        _resetCheckpoint = true;
        _optionsChanged = true;
    }


    void start() {
        LOCK(_mutex);
        _retainSelf = this;     // keep myself from being freed until the replicator stops
        if (_optionsChanged) {
            c4repl_setOptions(_c4repl, encodeOptions());
            _optionsChanged = false;
            _resetCheckpoint = false;
        }
        c4repl_start(_c4repl);
        _status = c4repl_getStatus(_c4repl);
    }


    CBLReplicatorStatus status() {
        return external(c4repl_getStatus(_c4repl));
    }


    CBLListenerToken* addChangeListener(CBLReplicatorChangeListener listener, void *context) {
        LOCK(_mutex);
        return _changeListeners.add(listener, context);
    }


    CBLListenerToken* addDocumentListener(CBLReplicatedDocumentListener listener, void *context) {
        LOCK(_mutex);
        if (_docListeners.empty())
            _optionsChanged = true;
        return _docListeners.add(listener, context);
    }

private:

    alloc_slice encodeOptions() {
        Encoder enc;
        enc.beginDict();
        _conf.writeOptions(enc);
        if (_resetCheckpoint)
            enc[slice(kC4ReplicatorResetCheckpoint)] = true;
        if (!_docListeners.empty())
            enc[slice(kC4ReplicatorOptionProgressLevel)] = 1;
        enc.endDict();
        return enc.finish();
    }


    void _statusChanged(const C4ReplicatorStatus &status) {
        LOCK(_mutex);
        C4Log("StatusChanged: level=%d, err=%d", status.level, status.error.code);
        _status = status;

        if (!_changeListeners.empty()) {
            auto cblStatus = external(status);
            _changeListeners.call(this, &cblStatus);
        } else if (status.error.code) {
            char buf[256];
            C4Warn("No listener to receive error from CBLReplicator %p: %s",
                   this, c4error_getDescriptionC(status.error, buf, sizeof(buf)));
        }

        if (status.level == kC4Stopped)
            _retainSelf = nullptr;  // Undoes the retain in `start`; now I can be freed
    }


    void _documentsEnded(bool pushing, size_t numDocs, const C4DocumentEnded* c4Docs[]) {
        LOCK(_mutex);
        if (_docListeners.empty())
            return;
        vector<CBLReplicatedDocument> docs(numDocs);
        auto src = &c4Docs[0];
        for (auto dst = docs.begin(); dst != docs.end(); ++dst, ++src) {
            dst->ID = (const char*)(*src)->docID.buf;
            dst->flags = 0;
            if ((*src)->flags & kRevDeleted)
                dst->flags |= kCBLDocumentFlagsDeleted;
            if ((*src)->flags & kRevPurged)
                dst->flags |= kCBLDocumentFlagsAccessRemoved;
            dst->error = external((*src)->error);
        }
        _docListeners.call(this, pushing, unsigned(numDocs), docs.data());
    }


    bool _filter(slice docID, C4RevisionFlags flags, Dict body, bool pushing) {
        Retained<CBLDocument> doc = new CBLDocument(_conf.database, string(docID), flags, body);
        CBLReplicationFilter filter = pushing ? _conf.pushFilter : _conf.pullFilter;
        return filter(_conf.context, doc, (flags & kRevDeleted) != 0);
    }


    recursive_mutex _mutex;
    ReplicatorConfiguration const _conf;
    Retained<CBLDatabase> _db;
    c4::ref<C4Replicator> _c4repl;
    C4ReplicatorStatus _status {kC4Stopped};
    bool _optionsChanged {false};
    bool _resetCheckpoint {false};
    Retained<CBLReplicator> _retainSelf;

    cbl_internal::Listeners<CBLReplicatorChangeListener> _changeListeners;
    cbl_internal::Listeners<CBLReplicatedDocumentListener> _docListeners;
};


#pragma mark - C API:


CBLEndpoint* CBLEndpoint_NewWithURL(const char *url _cbl_nonnull) CBLAPI {
    return new CBLURLEndpoint(url);
}

#ifdef COUCHBASE_ENTERPRISE
CBLEndpoint* CBLEndpoint_NewWithLocalDB(CBLDatabase* db) CBLAPI {
    return new CBLLocalEndpoint(db);
}
#endif

void CBLEndpoint_Free(CBLEndpoint *endpoint) CBLAPI {
    delete endpoint;
}

CBLAuthenticator* CBLAuth_NewBasic(const char *username, const char *password) CBLAPI {
    return new BasicAuthenticator(username, password);
}

CBLAuthenticator* CBLAuth_NewSession(const char *sessionID, const char *cookieName) CBLAPI {
    return new SessionAuthenticator(sessionID, cookieName);
}

void CBLAuth_Free(CBLAuthenticator *auth) CBLAPI {
    delete auth;
}

CBLReplicator* CBLReplicator_New(const CBLReplicatorConfiguration* conf, CBLError *outError) CBLAPI {
    return validated(new CBLReplicator(conf), outError);
}

const CBLReplicatorConfiguration* CBLReplicator_Config(CBLReplicator* repl) CBLAPI {
    return repl->configuration();
}

CBLReplicatorStatus CBLReplicator_Status(CBLReplicator* repl) CBLAPI {
    return repl->status();
}

void CBLReplicator_Start(CBLReplicator* repl) CBLAPI            {repl->start();}
void CBLReplicator_Stop(CBLReplicator* repl) CBLAPI             {repl->stop();}
void CBLReplicator_ResetCheckpoint(CBLReplicator* repl) CBLAPI  {repl->resetCheckpoint();}
void CBLReplicator_SetHostReachable(CBLReplicator* repl, bool r) CBLAPI {repl->setHostReachable(r);}
void CBLReplicator_SetSuspended(CBLReplicator* repl, bool sus) CBLAPI   {repl->setSuspended(sus);}

CBLListenerToken* CBLReplicator_AddChangeListener(CBLReplicator* repl,
                                                  CBLReplicatorChangeListener listener,
                                                  void *context) CBLAPI
{
    return repl->addChangeListener(listener, context);
}

CBLListenerToken* CBLReplicator_AddDocumentListener(CBLReplicator* repl,
                                                    CBLReplicatedDocumentListener listener,
                                                    void *context) CBLAPI
{
    return repl->addDocumentListener(listener, context);
}
