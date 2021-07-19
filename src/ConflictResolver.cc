//
// ConflictResolver.cc
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

#include "ConflictResolver.hh"
#include "CBLReplicator_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "Internal.hh"
#include "c4DocEnumerator.hh"
#include "StringUtil.hh"
#include "Stopwatch.hh"
#include <string>
#include "betterassert.hh"


static const CBLDocument* defaultConflictResolver(void *context,
                                                  FLString documentID,
                                                  const CBLDocument *localDocument,
                                                  const CBLDocument *remoteDocument)
{
    return localDocument;
}

const CBLConflictResolver CBLDefaultConflictResolver = &defaultConflictResolver;


namespace cbl_internal {
    using namespace fleece;
    using namespace litecore;

    ConflictResolver::ConflictResolver(CBLDatabase *db,
                                       CBLConflictResolver customResolver,
                                       void* context,
                                       alloc_slice docID,
                                       alloc_slice revID)
    :_db(db)
    ,_clientResolver(customResolver)
    ,_clientResolverContext(context)
    ,_docID(move(docID))
    ,_revID(move(revID))
    {
        //SyncLog(Info, "ConflictResolver %p on %.*s", this, _docID.c_str());
    }


    ConflictResolver::ConflictResolver(CBLDatabase *db,
                                       CBLConflictResolver customResolver,
                                       void* context,
                                       const C4DocumentEnded &docEnded)
    :ConflictResolver(db, customResolver, context, docEnded.docID, docEnded.revID)
    { }


    void ConflictResolver::runAsync(CompletionHandler completionHandler) noexcept {
        assert(completionHandler);
        _completionHandler = completionHandler;
        SyncLog(Info, "Scheduling async resolution of conflict in doc '%.*s'",
                FMTSLICE(_docID));
        c4_runAsyncTask([](void *context) { ((ConflictResolver*)context)->runNow(); },
                        this);
    }


    // Performs conflict resolution. Returns true on success, false on failure. Sets _error.
    bool ConflictResolver::runNow() {
        bool ok, inConflict = false;
        int retryCount = 0;
        try {
            do {
                // Create a CBLDocument that reflects the conflict revision:
                auto conflict = _db->getMutableDocument(_docID);
                if (!conflict) {
                    SyncLog(Info, "Doc '%.*s' no longer exists, no conflict to resolve",
                            FMTSLICE(_docID));
                    return true;
                }

                if (_revID) {
                    ok = conflict->selectRevision(_revID) &&
                         (conflict->revisionFlags() & (kRevLeaf|kRevIsConflict)) ==
                                                      (kRevLeaf|kRevIsConflict);
                } else {
                    ok = conflict->selectNextConflictingRevision();
                    _revID = conflict->revisionID();
                }
                if (!ok) {
                    // Revision is gone or not a leaf: Conflict must be resolved, so stop
                    SyncLog(Info, "Conflict in doc '%.*s' already resolved, nothing to do",
                            FMTSLICE(_docID));
                    return true;
                }

                // Now resolve the conflict:
                if (_clientResolver)
                    ok = customResolve(conflict);
                else {
                    // FIXME: CBL-2144:
                    auto resolved = _db->getDocument(_docID, true);
                    if (resolved && resolved->revisionFlags() & kRevDeleted)
                        resolved = nullptr;
                    ok = conflict->resolveConflict(CBLDocument::Resolution::useLocal, resolved);
                }

                if (ok) {
                    _revID = conflict->revisionID();
                    _flags = conflict->revisionFlags();
                } else {
                    _error = external(C4Error::make(LiteCoreDomain, kC4ErrorConflict));
                    // If a local revision is saved at the same time we'll fail with a conflict, so retry:
                    inConflict = (++retryCount < 10);
                    if (inConflict) {
                        SyncLog(Warning, "%s conflict resolution of doc '%.*s' conflicted with newer saved"
                                " revision; retrying...",
                                (_clientResolver ? "Custom" : "Default"), FMTSLICE(_docID));
                    }
                }
            } while (inConflict);
        } catch (...) {
            C4Error::fromCurrentException(internal(&_error));
            ok = false;
        }

        if (ok) {
            SyncLog(Info, "Successfully resolved and saved doc '%.*s'", FMTSLICE(_docID));
            _error = {};
        } else {
            SyncLog(Error, "%s conflict resolution of doc '%.*s' failed: %s\n%s",
                    (_clientResolver ? "Custom" : "Default"),
                    FMTSLICE(_docID),
                    internal(_error).description().c_str(),
                    internal(_error).backtrace().c_str());
        }
        
        if (_completionHandler)
            _completionHandler(this);       // the handler will most likely delete me
        return ok;
    }


    // Performs custom conflict resolution.
    bool ConflictResolver::customResolve(CBLDocument *conflict) {
        CBLDocument *remoteDoc = conflict;
        if (remoteDoc->revisionFlags() & kRevDeleted)
            remoteDoc = nullptr;
        auto localDoc = _db->getDocument(_docID, true);
        if (localDoc && localDoc->revisionFlags() & kRevDeleted)
            localDoc = nullptr;

        // Call the custom resolver (this could take a long time to return)
        SyncLog(Verbose, "Calling custom conflict resolver for doc '%.*s' ...",
                FMTSLICE(_docID));
        Stopwatch st;
        RetainedConst<CBLDocument> resolved;
        try {
            // Note: Adopt will not increase the retained count
            resolved = adopt(_clientResolver(_clientResolverContext, _docID, localDoc, remoteDoc));
        } catch (...) {
            C4Error::raise(LiteCoreDomain, kC4ErrorUnexpectedError,
                           "Custom conflict handler threw an exception");
        }
        SyncLog(Info, "Custom conflict resolver for '%.*s' took %.0fms",
                FMTSLICE(_docID), st.elapsedMS());

        // Determine the resolution type:
        CBLDocument::Resolution resolution;
        if (resolved == localDoc) {
            resolution = CBLDocument::Resolution::useLocal;
            CBLDocument_Retain(localDoc); // Match number of objects (localDoc and resolved)
        } else if (resolved == conflict) {
            resolution = CBLDocument::Resolution::useRemote;
            CBLDocument_Retain(remoteDoc); // Match number of objects (remoteDoc and resolved)
        } else {
            if (resolved) {
                // Sanity check the resolved document:
                if (resolved->database() && resolved->database() != _db) {
                    C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter,
                                   "CBLDocument returned from custom conflict resolver belongs to"
                                   " wrong database");
                }
                if (resolved->docID() != _docID) {
                    C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter,
                                   "CBLDocument returned from custom conflict resolver has wrong"
                                   " docID `%.*s` (should be %.*s)",
                                   FMTSLICE(resolved->docID()), FMTSLICE(_docID));
                }
            }
            resolution = CBLDocument::Resolution::useMerge;
        }

        // Actually resolve the conflict & save the document:
        return conflict->resolveConflict(resolution, resolved);
    }

    CBLReplicatedDocument ConflictResolver::result() const {
        CBLReplicatedDocument doc = {};
        doc.ID = _docID;
        doc.error = _error;
        if (_flags & kRevDeleted)
            doc.flags |= kCBLDocumentFlagsDeleted;
        if (_flags & kRevPurged)
            doc.flags |= kCBLDocumentFlagsAccessRemoved;
        return doc;
    }


#pragma mark - ALL CONFLICTS RESOLVER:


    AllConflictsResolver::AllConflictsResolver(CBLDatabase *db,
                                               CBLConflictResolver resolver, void *context)
    :_db(db)
    ,_clientResolver(resolver)
    ,_clientResolverContext(context)
    { }


    void AllConflictsResolver::runNow() {
        while (next()) {
            alloc_slice docID = _enum->documentInfo().docID;
            ConflictResolver resolver(_db, _clientResolver, _clientResolverContext, docID);
            resolver.runNow();
        }
    }


    bool AllConflictsResolver::next() {
        return _db->useLocked<bool>([&](C4Database *c4db) {
            if (!_enum) {
                // Flags value of 0 means without kC4IncludeNonConflicted, i.e. only conflicted.
                _enum = make_unique<C4DocEnumerator>(c4db, C4EnumeratorOptions{ 0 });
            }
            return _enum->next();
        });
    }

}
