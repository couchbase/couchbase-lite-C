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
#include "c4DocEnumerator.h"
#include "c4.hh"
#include "StringUtil.hh"
#include "Stopwatch.hh"
#include <string>


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
                                       CBLConflictResolver customResolver, void* context,
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
                                       CBLConflictResolver customResolver, void* context,
                                       const C4DocumentEnded &docEnded)
    :ConflictResolver(db, customResolver, context, docEnded.docID, docEnded.revID)
    { }


    void ConflictResolver::runAsync(CompletionHandler completionHandler) noexcept {
        assert(completionHandler);
        _completionHandler = completionHandler;
        SyncLog(Info, "Scheduling async resolution of conflict in doc '%.*s'",
                FMTSLICE(_docID));
        c4_runAsyncTask([](void *context) { ((ConflictResolver*)context)->_runAsyncNow(); },
                        this);
    }


    void ConflictResolver::_runAsyncNow() noexcept {
        try {
            runNow();
        } catch (std::exception &x) {
            errorFromException(&x, "Conflict resolution");
        } catch (...) {
            errorFromException(nullptr, "Conflict resolution");
        }
        _completionHandler(this);       // the handler will most likely delete me
    }


    // Performs conflict resolution. Returns true on success, false on failure. Sets _error.
    bool ConflictResolver::runNow() {
        bool ok, inConflict = false;
        int retryCount = 0;
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
            else
                ok = conflict->resolveConflict(CBLDocument::Resolution::useLocal, nullptr);

            if (ok) {
                _revID = conflict->revisionID();
                _flags = conflict->revisionFlags();
            } else {
                _error = external(c4error_make(LiteCoreDomain, kC4ErrorConflict, {}));
                // If a local revision is saved at the same time we'll fail with a conflict, so retry:
                inConflict = (++retryCount < 10);
                if (inConflict) {
                    SyncLog(Warning, "%s conflict resolution of doc '%.*s' conflicted with newer saved"
                            " revision; retrying...",
                            (_clientResolver ? "Custom" : "Default"), FMTSLICE(_docID));
                }
            }
        } while (inConflict);

        if (ok) {
            SyncLog(Info, "Successfully resolved and saved doc '%.*s'", FMTSLICE(_docID));
            _error = {};
        } else {
            alloc_slice backtrace = c4error_getBacktrace(internal(_error));
            SyncLog(Error, "%s conflict resolution of doc '%.*s' failed: %s\n%.*s",
                    (_clientResolver ? "Custom" : "Default"),
                    FMTSLICE(_docID),
                    c4error_descriptionStr(internal(_error)),
                    FMTSLICE(backtrace));
        }
        return ok;
    }


    // Performs custom conflict resolution.
    bool ConflictResolver::customResolve(CBLDocument *conflict) {
        CBLDocument *otherDoc = conflict;
        if (otherDoc->revisionFlags() & kRevDeleted)
            otherDoc = nullptr;
        auto myDoc = _db->getDocument(_docID, true);
        if (myDoc && myDoc->revisionFlags() & kRevDeleted)
            myDoc = nullptr;

        // Call the custom resolver (this could take a long time to return)
        SyncLog(Verbose, "Calling custom conflict resolver for doc '%.*s' ...",
                FMTSLICE(_docID));
        Stopwatch st;
        RetainedConst<CBLDocument> resolved;
        try {
            resolved = adopt(_clientResolver(_clientResolverContext, _docID, myDoc, otherDoc));
        } catch (...) {
            C4Error::raise(LiteCoreDomain, kC4ErrorUnexpectedError,
                           "Custom conflict handler threw an exception");
        }
        SyncLog(Info, "Custom conflict resolver for '%.*s' took %.0fms",
                FMTSLICE(_docID), st.elapsedMS());

        // Determine the resolution type:
        CBLDocument::Resolution resolution;
        if (resolved == myDoc) {
            resolution = CBLDocument::Resolution::useLocal;
            resolved = nullptr;
        } else if (resolved == conflict) {
            resolution = CBLDocument::Resolution::useRemote;
            resolved = nullptr;
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


    void ConflictResolver::errorFromException(const std::exception *x, const char *what) {
        string message = what;
        if (x)
            (message += " threw an exception: ") += x->what();
        else
            message += " threw an unknown exception";
        SyncLog(Error, "%s", message.c_str());
        c4error_return(LiteCoreDomain, kC4ErrorUnexpectedError, slice(message), internal(&_error));
    }


#pragma mark - ALL CONFLICTS RESOLVER:


    AllConflictsResolver::AllConflictsResolver(CBLDatabase *db,
                                               CBLConflictResolver resolver, void *context)
    :_db(db)
    ,_clientResolver(resolver)
    ,_clientResolverContext(context)
    { }


    void AllConflictsResolver::runNow() {
        C4DocumentEnded doc;
        C4Error c4err;
        alloc_slice docID;
        while (nullslice != (docID = next(doc, &c4err))) {
            ConflictResolver resolver(_db, _clientResolver, _clientResolverContext, docID);
            resolver.runNow();
        }
    }


    alloc_slice AllConflictsResolver::next(C4DocumentEnded &doc, C4Error *c4err) {
        return _db->use<alloc_slice>([&](C4Database *c4db) -> alloc_slice {
            if (!_enum) {
                C4EnumeratorOptions options = { 0 };  // i.e. without kC4IncludeNonConflicted
                _enum = c4db_enumerateAllDocs(c4db, &options, c4err);
                if (!_enum)
                    return nullslice;
            }

            if (!c4enum_next(_enum, c4err))
                return nullslice;

            C4DocumentInfo info;
            c4enum_getDocumentInfo(_enum, &info);
            return info.docID;
        });
    }

}
