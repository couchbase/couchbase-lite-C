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


static const CBLDocument* defaultConflictResolver(void *context,
                                            const char *documentID,
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
    ,_docID(docID)
    ,_revID(revID)
    {
        //SyncLog(Info, "ConflictResolver %p on %s", this, _docID.c_str());
    }


    ConflictResolver::ConflictResolver(CBLDatabase *db,
                                       CBLConflictResolver customResolver, void* context,
                                       const C4DocumentEnded &docEnded)
    :ConflictResolver(db, customResolver, context,
                      alloc_slice(docEnded.docID), alloc_slice(docEnded.revID))
    { }


    void ConflictResolver::runAsync(CompletionHandler completionHandler) noexcept {
        assert(completionHandler);
        _completionHandler = completionHandler;
        SyncLog(Info, "Scheduling async resolution of conflict in doc '%s'",
                _docID.c_str());
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
        bool ok, inConflict;
        int retryCount = 0;
        do {
            // Create a CBLDocument that reflects the conflict revision:
            Retained<CBLDocument> conflict = new CBLDocument(_db, _docID.c_str(), true, true);
            if (!conflict->exists()) {
                SyncLog(Info, "Doc '%s' no longer exists, no conflict to resolve",
                        _docID.c_str());
                return true;
            }

            if (_revID) {
                ok = conflict->selectRevision(_revID) &&
                     (conflict->revisionFlags() & (kRevLeaf|kRevIsConflict)) ==
                                                  (kRevLeaf|kRevIsConflict);
            } else {
                ok = conflict->selectNextConflictingRevision();
                _revID = slice(conflict->revisionID());
            }
            if (!ok) {
                // Revision is gone or not a leaf: Conflict must be resolved, so stop
                SyncLog(Info, "Conflict in doc '%s' already resolved, nothing to do",
                        _docID.c_str());
                return true;
            }

            // Now resolve the conflict:
            if (_clientResolver)
                ok = customResolve(conflict);
            else
                ok = conflict->resolveConflict(CBLDocument::Resolution::useLocal, nullptr, &_error);

            if (ok) {
                _revID = conflict->revisionID();
                _flags = conflict->revisionFlags();
            }

            // If a local revision is saved at the same time we'll fail with a conflict, so retry:
            inConflict = (!ok && internal(_error) == C4Error{LiteCoreDomain, kC4ErrorConflict}
                              && ++retryCount < 10);
            if (inConflict) {
                SyncLog(Warning, "%s conflict resolution of doc '%s' conflicted with newer saved"
                        " revision; retrying...",
                        (_clientResolver ? "Custom" : "Default"), _docID.c_str());
            }
        } while (inConflict);

        if (ok) {
            SyncLog(Info, "Successfully resolved and saved doc '%s'", _docID.c_str());
            _error = {};
        } else {
            SyncLog(Error, "%s conflict resolution of doc '%s' failed: %s",
                    (_clientResolver ? "Custom" : "Default"),
                    _docID.c_str(), c4error_descriptionStr(internal(_error)));
        }
        return ok;
    }


    // Performs custom conflict resolution.
    bool ConflictResolver::customResolve(CBLDocument *conflict) {
        CBLDocument *otherDoc = conflict;
        if (otherDoc->revisionFlags() & kRevDeleted)
            otherDoc = nullptr;
        RetainedConst<CBLDocument> myDoc = new CBLDocument(_db, _docID.c_str(), false);
        if (!myDoc->exists())
            myDoc = nullptr;

        // Call the custom resolver (this could take a long time to return)
        SyncLog(Verbose, "Calling custom conflict resolver for doc '%s' ...",
                _docID.c_str());
        Stopwatch st;
        const CBLDocument *resolved;
        try {
            resolved = _clientResolver(_clientResolverContext, _docID.c_str(), myDoc, otherDoc);
        } catch (std::exception &x) {
            errorFromException(&x, "Custom conflict resolver");
            return false;
        } catch (...) {
            errorFromException(nullptr, "Custom conflict resolver");
            return false;
        }
        SyncLog(Info, "Custom conflict resolver for '%s' took %.0fms",
                _docID.c_str(), st.elapsedMS());

        // Determine the resolution type:
        CBLDocument::Resolution resolution;
        if (resolved == myDoc) {
            resolution = CBLDocument::Resolution::useLocal;
            resolved = nullptr;
        } else if (resolved == conflict) {
            resolution = CBLDocument::Resolution::useRemote;
            resolved = nullptr;
        } else {
            resolution = CBLDocument::Resolution::useMerge;
            if (resolved) {
                // Sanity check the resolved document:
                if (resolved->database() && resolved->database() != _db) {
                    c4error_return(LiteCoreDomain, kC4ErrorInvalidParameter,
                                   "CBLDocument returned from custom conflict resolver belongs to"
                                   " wrong database"_sl, internal(&_error));
                    return false;
                }
                if (resolved->docID() != _docID) {
                    SyncLog(Warning, "CBLDocument returned from custom conflict resolver has wrong"
                            " docID `%s` (should be %s)",
                            resolved->docID(), _docID.c_str());
                }
            }
        }

        // Actually resolve the conflict & save the document:
        bool ok = conflict->resolveConflict(resolution, resolved, &_error);

        CBLDocument_Release(resolved);
        return ok;
    }


    CBLReplicatedDocument ConflictResolver::result() const {
        CBLReplicatedDocument doc = {};
        doc.ID = _docID.c_str();
        doc.error = _error;
        if (_flags & kRevDeleted)
            doc.flags |= kCBLDocumentFlagsDeleted;
        if (_flags & kRevPurged)
            doc.flags |= kCBLDocumentFlagsAccessRemoved;
        return doc;
    }


    void ConflictResolver::errorFromException(const std::exception *x, const string &what) {
        string message;
        if (x)
            message = what + " threw an exception: " + x->what();
        else
            message = what + " threw an unknown exception";
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
