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
#include "c4Replicator.hh"
#include "c4Private.h"
#include "StringUtil.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

CBL_ASSUME_NONNULL_BEGIN

#define SyncLog(LEVEL, MSG, ...) C4LogToAt(kC4SyncLog, kC4Log ## LEVEL, MSG, ##__VA_ARGS__)


extern "C" {
    void C4RegisterBuiltInWebSocket();
}

static CBLReplicatorStatus external(const C4ReplicatorStatus &c4status) {
    float complete;
    if (c4status.progress.unitsTotal == 0 &&
        (c4status.level == kC4Idle || c4status.level == kC4Stopped) && c4status.error.code == 0) {
        complete = 1.0; // When the replicator is idle or stopped, return as completed if having no changes to replicate
    } else {
        complete = c4status.progress.unitsCompleted / std::max(float(c4status.progress.unitsTotal), 1.0f);
        complete = std::min(complete, 1.0f); // CBL-2610 : Workaround for unitsCompleted > unitsTotal
    }
    
    return {
        CBLReplicatorActivityLevel(min(c4status.level, (C4ReplicatorActivityLevel)kC4Busy)),   // don't expose kC4Stopping
        {
            complete,
            c4status.progress.documentCount
        },
        external(c4status.error)
    };
}


struct CBLReplicator final : public CBLRefCounted, public CBLStoppable {
public:
    CBLReplicator(const CBLReplicatorConfiguration &conf)
    :_conf(conf)
    ,_db(conf.database)
    {
        // One-time initialization of network transport:
        static once_flag once;
        call_once(once, std::bind(&C4RegisterBuiltInWebSocket));

        // Set up the LiteCore replicator parameters:
        _conf.validate();
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
            params.pushFilter = [](C4String collectionName,
                                   C4String docID,
                                   C4String revID,
                                   C4RevisionFlags flags,
                                   FLDict body,
                                   void* ctx)
            {
                return ((CBLReplicator*)ctx)->_filter(docID, revID, flags, body, true);
            };
        }
        if (_conf.pullFilter) {
            params.validationFunc = [](C4String collectionName,
                                       C4String docID,
                                       C4String revID,
                                       C4RevisionFlags flags,
                                       FLDict body,
                                       void* ctx)
            {
                return ((CBLReplicator*)ctx)->_filter(docID, revID, flags, body, false);
            };
        }
        
#ifdef COUCHBASE_ENTERPRISE
        
        if (_conf.propertyEncryptor) {
            params.propertyEncryptor = [](void* ctx,
                                          C4String documentID,
                                          FLDict properties,
                                          C4String keyPath,
                                          C4Slice input,
                                          C4StringResult* outAlgorithm,
                                          C4StringResult* outKeyID,
                                          C4Error* outError)
            {
                return ((CBLReplicator*)ctx)->_encrypt(documentID, properties, keyPath, input,
                                                       outAlgorithm, outKeyID, outError);
            };
        }
        
        if (_conf.propertyDecryptor) {
            params.propertyDecryptor = [](void* ctx,
                                          C4String documentID,
                                          FLDict properties,
                                          C4String keyPath,
                                          C4Slice input,
                                          C4String algorithm,
                                          C4String keyID,
                                          C4Error* outError)
            {
                return ((CBLReplicator*)ctx)->_decrypt(documentID, properties, keyPath, input,
                                                       algorithm, keyID, outError);
            };
        }
        
#endif

        // Encode replicator options dict:
        alloc_slice options = encodeOptions();
        params.optionsDictFleece = options;

        // Create the LiteCore replicator:
        _conf.database->useLocked([&](C4Database *c4db) {
#ifdef COUCHBASE_ENTERPRISE
            if (_conf.endpoint->otherLocalDB()) {
                _c4repl = c4db->newLocalReplicator(_conf.endpoint->otherLocalDB()->useLocked().get(),
                                                   params);
            } else
#endif
            {
                _c4repl = c4db->newReplicator(_conf.endpoint->remoteAddress(),
                                              _conf.endpoint->remoteDatabaseName(),
                                              params);
            }
        });
        _c4status = _c4repl->getStatus();
        _useInitialStatus = true;
    }
    

    const ReplicatorConfiguration* configuration() const    {return &_conf;}
    void setHostReachable(bool reachable)                   {_c4repl->setHostReachable(reachable);}
    void setSuspended(bool suspended)                       {_c4repl->setSuspended(suspended);}
    void stop() override                                    {_c4repl->stop();}


    void start(bool reset) {
        LOCK(_mutex);
        _retainSelf = this;     // keep myself from being freed until the replicator stops
        _useInitialStatus = false;
        
        if (_db->registerStoppable(this))
            _c4repl->start(reset);
        else
            CBL_Log(kCBLLogDomainReplicator, kCBLLogWarning,
                    "Couldn't start the replicator as the database is closing or closed.");
    }


    CBLReplicatorStatus status() {
        LOCK(_mutex);
        if (_useInitialStatus)
            return {}; // Allow to return initial status with zero progress.complete
        return effectiveStatus(_c4repl->getStatus());
    }


    MutableDict pendingDocumentIDs() {
        alloc_slice arrayData(_c4repl->pendingDocIDs());
        if (!arrayData)
            return nullptr;

        MutableDict result = MutableDict::newDict();
        if (arrayData) {
            Doc doc(arrayData, kFLTrusted);
            for (Array::iterator i(doc.asArray()); i; ++i)
                result.set(i->asString(), true);
        }
        return result;
    }


    bool isDocumentPending(FLSlice docID) {
        return _c4repl->isDocumentPending(docID);
    }


    Retained<CBLListenerToken> addChangeListener(CBLReplicatorChangeListener listener,
                                                 void *context)
    {
        LOCK(_mutex);
        return _changeListeners.add(listener, context);
    }


    Retained<CBLListenerToken> addDocumentListener(CBLDocumentReplicationListener listener,
                                                   void *context)
    {
        LOCK(_mutex);
        if (_docListeners.empty()) {
            _c4repl->setProgressLevel(kC4ReplProgressPerDocument);
            _progressLevel = kC4ReplProgressPerDocument;
        }
        return _docListeners.add(listener, context);
    }

private:

    alloc_slice encodeOptions() {
        Encoder enc;
        enc.beginDict();
        _conf.writeOptions(enc);
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
        
        SyncLog(Info, "CBLReplicator status: %s, progress=%llu/%llu, flag=%d, error=%d/%d (effective status=%s, completed=%.2f%%, docs=%llu)",
                kC4ReplicatorActivityLevelNames[c4status.level],
                c4status.progress.unitsCompleted, c4status.progress.unitsTotal,
                c4status.flags, c4status.error.domain, c4status.error.code,
                kC4ReplicatorActivityLevelNames[cblStatus.activity],
                cblStatus.progress.complete, cblStatus.progress.documentCount);
        
        if (!_changeListeners.empty()) {
            _changeListeners.call(this, &cblStatus);
        } else if (cblStatus.error.code) {
            char buf[256];
            SyncLog(Warning, "No listener to receive error from CBLReplicator %p: %s",
                    this, c4error_getDescriptionC(c4status.error, buf, sizeof(buf)));
        }

        if (cblStatus.activity == kCBLReplicatorStopped) {
            _db->unregisterStoppable(this);
            _retainSelf = nullptr;  // Undoes the retain in `start`; now I can be freed
        }
    }


    void _documentsEnded(bool pushing,
                         size_t numDocs,
                         const C4DocumentEnded* _cbl_nonnull c4Docs[_cbl_nonnull])
    {
        LOCK(_mutex);
        std::unique_ptr<std::vector<CBLReplicatedDocument>> docs;
        if (!_docListeners.empty()) {
            docs = std::make_unique<std::vector<CBLReplicatedDocument>>();
            docs->reserve(numDocs);
        } else if (_progressLevel != kC4ReplProgressOverall) {
            _c4repl->setProgressLevel(kC4ReplProgressOverall);
            _progressLevel = kC4ReplProgressOverall;
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
                doc.ID = src.docID;
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
        Retained<CBLDocument> doc = new CBLDocument(_conf.database, docID, revID, flags, body);
        CBLReplicationFilter filter = pushing ? _conf.pushFilter : _conf.pullFilter;
        
        CBLDocumentFlags docFlags = 0;
        if (flags & kRevDeleted)
            docFlags |= kCBLDocumentFlagsDeleted;
        if (flags & kRevPurged)
            docFlags |= kCBLDocumentFlagsAccessRemoved;
        
        return filter(_conf.context, doc, docFlags);
    }

#ifdef COUCHBASE_ENTERPRISE
    
    C4SliceResult _encrypt(C4String documentID, FLDict properties, C4String keyPath, C4Slice input,
                           C4StringResult* outAlgorithm, C4StringResult* outKeyID, C4Error* outError)
    {
        CBLError error = {};
        auto encryptor = _conf.propertyEncryptor;
        auto result = encryptor(_conf.context, documentID, properties, keyPath, input,
                                outAlgorithm, outKeyID, &error);
        *outError = internal(error);
        return result;
    }
    
    C4SliceResult _decrypt(C4String documentID, FLDict properties, C4String keyPath, C4Slice input,
                           C4String algorithm, C4String keyID, C4Error* outError)
    {
        CBLError error = {};
        auto decryptor = _conf.propertyDecryptor;
        auto result = decryptor(_conf.context, documentID, properties, keyPath, input,
                                algorithm, keyID, &error);
        *outError = internal(error);
        return result;
    }
    
#endif

    recursive_mutex                             _mutex;
    ReplicatorConfiguration const               _conf;
    Retained<CBLDatabase>                       _db;
    Retained<C4Replicator>                      _c4repl;
    bool                                        _useInitialStatus;  // For returning status before first start
    C4ReplicatorStatus                          _c4status {kC4Stopped};
    Retained<CBLReplicator>                     _retainSelf;
    int                                         _activeConflictResolvers {0};
    Listeners<CBLReplicatorChangeListener>      _changeListeners;
    Listeners<CBLDocumentReplicationListener>   _docListeners;
    C4ReplicatorProgressLevel                   _progressLevel {kC4ReplProgressOverall};
};

CBL_ASSUME_NONNULL_END
