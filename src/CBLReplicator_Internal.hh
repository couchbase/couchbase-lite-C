//
//  CBLReplicator_Internal.hh
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
#include "ConflictResolver.hh"
#include "Internal.hh"
#include "Util.hh"
#include "c4.hh"
#include "c4Replicator.h"
#include "c4Private.h"
#include "StringUtil.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <atomic>
#include <mutex>
#include <algorithm>


#define SyncLog(LEVEL, MSG, ...) C4LogToAt(kC4SyncLog, kC4Log ## LEVEL, MSG, ##__VA_ARGS__)


extern "C" {
    void C4RegisterBuiltInWebSocket();
}

static CBLReplicatorStatus external(const C4ReplicatorStatus &c4status) {
    return {
        CBLReplicatorActivityLevel(c4status.level),
        {
            c4status.progress.unitsCompleted / std::max(float(c4status.progress.unitsTotal), 1.0f),
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
        call_once(once, std::bind(&C4RegisterBuiltInWebSocket));

        // Set up the LiteCore replicator parameters:
        if (!_conf.validate(external(&_c4status.error)))
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
                                   C4String revID,
                                   C4RevisionFlags flags,
                                   FLDict body,
                                   void* ctx)
            {
                return ((CBLReplicator*)ctx)->_filter(docID, revID, flags, body, true);
            };
        }
        if (_conf.pullFilter) {
            params.validationFunc = [](C4String docID,
                                       C4String revID,
                                       C4RevisionFlags flags,
                                       FLDict body,
                                       void* ctx)
            {
                return ((CBLReplicator*)ctx)->_filter(docID, revID, flags, body, false);
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
                                          &_c4status.error);
            } else
#endif
            {
                _c4repl = c4repl_new(c4db,
                                     _conf.endpoint->remoteAddress(),
                                     _conf.endpoint->remoteDatabaseName(),
                                     params,
                                     &_c4status.error);
            }
        });
        if (_c4repl)
            _c4status = c4repl_getStatus(_c4repl);
    }


    bool validate(CBLError *err) const {
        if (!_c4repl) {
            if (err) *err = external(_c4status.error);
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
    }


    void start() {
        LOCK(_mutex);
        _retainSelf = this;     // keep myself from being freed until the replicator stops
        if (_optionsChanged) {
            c4repl_setOptions(_c4repl, encodeOptions());
            _optionsChanged = false;
        }
        c4repl_start(_c4repl, _resetCheckpoint);
        _resetCheckpoint = false;
    }



    CBLReplicatorStatus status() {
        return effectiveStatus(c4repl_getStatus(_c4repl));
    }


    MutableDict pendingDocumentIDs(CBLError *outError) {
        C4Error c4err;
        alloc_slice arrayData(c4repl_getPendingDocIDs(_c4repl, &c4err));
        if (outError)
            *outError = external(c4err);
        if (!arrayData && c4err.code != 0)
            return nullptr;

        MutableDict result = MutableDict::newDict();
        if (arrayData) {
            Doc doc(arrayData, kFLTrusted);
            for (Array::iterator i(doc.asArray()); i; ++i)
                result.set(i->asString(), true);
        }
        return result;
    }


    bool isDocumentPending(FLSlice docID, CBLError *outError) {
        return c4repl_isDocumentPending(_c4repl, docID, internal(outError));
    }


    Retained<CBLListenerToken> addChangeListener(CBLReplicatorChangeListener listener,
                                                 void *context)
    {
        LOCK(_mutex);
        return _changeListeners.add(listener, context);
    }


    Retained<CBLListenerToken> addDocumentListener(CBLReplicatedDocumentListener listener,
                                                   void *context)
    {
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
        if (!_docListeners.empty())
            enc[slice(kC4ReplicatorOptionProgressLevel)] = 1;
        enc.endDict();
        return enc.finish();
    }


    CBLReplicatorStatus effectiveStatus(C4ReplicatorStatus c4status) {
        LOCK(_mutex);
        auto eff = external(c4status);
        // Bump effective status to Busy if conflict resolvers are running, but pass
        // Offline status through.
        if (_activeConflictResolvers > 0) {
            if (eff.activity != kCBLReplicatorOffline)
                eff.activity = kCBLReplicatorBusy;
        }
        return eff;
    }


    void bumpConflictResolverCount(int delta) {
        auto curActivity = effectiveStatus(_c4status).activity;
        _activeConflictResolvers += delta;
        if (effectiveStatus(_c4status).activity != curActivity)
            _statusChanged(_c4status);
    }


    void _statusChanged(C4ReplicatorStatus c4status) {
        LOCK(_mutex);
        _c4status = c4status;
        auto cblStatus = effectiveStatus(c4status);

        if (!_changeListeners.empty()) {
            _changeListeners.call(this, &cblStatus);
        } else if (cblStatus.error.code) {
            char buf[256];
            SyncLog(Warning, "No listener to receive error from CBLReplicator %p: %s",
                    this, c4error_getDescriptionC(c4status.error, buf, sizeof(buf)));
        }

        if (cblStatus.activity == kCBLReplicatorStopped)
            _retainSelf = nullptr;  // Undoes the retain in `start`; now I can be freed
    }


    void _documentsEnded(bool pushing, size_t numDocs, const C4DocumentEnded* c4Docs[]) {
        LOCK(_mutex);
        std::unique_ptr<std::vector<CBLReplicatedDocument>> docs;
        if (!_docListeners.empty()) {
            docs = std::make_unique<std::vector<CBLReplicatedDocument>>();
            docs->reserve(numDocs);
        }
        for (size_t i = 0; i < numDocs; ++i) {
            auto src = *c4Docs[i];
            if (!pushing && src.flags & kRevIsConflict) {
                // Conflict -- start an async resolver task:
                auto r = new ConflictResolver(_db, _conf.conflictResolver, _conf.context, src);
                bumpConflictResolverCount(1);
                r->runAsync( bind(&CBLReplicator::_conflictResolverFinished, this, std::placeholders::_1) );
            } else if (docs) {
                // Otherwise add to list of changes to notify:
                CBLReplicatedDocument doc = {};
                doc.ID = (const char*)src.docID.buf;
                doc.error = external(src.error);
                doc.flags = 0;
                if (src.flags & kRevDeleted)
                    doc.flags |= kCBLDocumentFlagsDeleted;
                if (src.flags & kRevPurged)
                    doc.flags |= kCBLDocumentFlagsAccessRemoved;
                docs->push_back(doc);
            }
        }
        if (docs)
            _docListeners.call(this, pushing, unsigned(docs->size()), docs->data());
    }


    void _conflictResolverFinished(ConflictResolver *resolver) {
        CBLReplicatedDocument doc = resolver->result();
        _docListeners.call(this, false, 1, &doc);
        delete resolver;

        LOCK(_mutex);
        bumpConflictResolverCount(-1);
    }


    bool _filter(slice docID, slice revID, C4RevisionFlags flags, Dict body, bool pushing) {
        Retained<CBLDocument> doc = new CBLDocument(_conf.database, string(docID), revID, flags, body);
        CBLReplicationFilter filter = pushing ? _conf.pushFilter : _conf.pullFilter;
        return filter(_conf.context, doc, (flags & kRevDeleted) != 0);
    }


    recursive_mutex                 _mutex;
    ReplicatorConfiguration const   _conf;
    Retained<CBLDatabase>           _db;
    c4::ref<C4Replicator>           _c4repl;
    C4ReplicatorStatus              _c4status {kC4Stopped};
    bool                            _optionsChanged {false};
    bool                            _resetCheckpoint {false};
    Retained<CBLReplicator>         _retainSelf;
    int                             _activeConflictResolvers {0};
    Listeners<CBLReplicatorChangeListener>    _changeListeners;
    Listeners<CBLReplicatedDocumentListener>  _docListeners;
};
