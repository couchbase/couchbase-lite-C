//
// ConflictResolver.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "CBLReplicatorConfig.hh"
#include <functional>

CBL_ASSUME_NONNULL_BEGIN

struct CBLDatabase;

namespace cbl_internal {
    using namespace std;
    using namespace fleece;

    /** Resolves a replication conflict in a document, synchronously or asynchronously. */
    class ConflictResolver {
    public:
        /// Basic constructor.
        ConflictResolver(CBLCollection *collection,
                         CBLConflictResolver _cbl_nullable customResolver,
                         void* _cbl_nullable context,
                         alloc_slice docID);

        ConflictResolver(CBLCollection*,
                         CBLConflictResolver _cbl_nullable,
                         void* _cbl_nullable context,
                         const C4DocumentEnded&);

        using CompletionHandler = function<void(ConflictResolver*)>;

        /// Schedules async conflict resolution.
        /// @param handler  Completion handler to call when finished.
        void runAsync(CompletionHandler handler) noexcept;

        /// Performs resolution synchronously.
        /// @return true on success, false on failure.
        bool runNow();

        /// The result of the resolution, as a CBLReplicatedDocument struct suitable for sending
        /// to the replicator's progress listener.
        CBLReplicatedDocument result() const;

    private:
        bool _runNow();
        bool defaultResolve(CBLDocument *conflict);
        bool customResolve(CBLDocument *conflict);

        Retained<CBLCollection>  _collection;
        CBLConflictResolver _cbl_nullable _clientResolver;
        void* _cbl_nullable     _clientResolverContext;
        alloc_slice const       _docID;
        C4RevisionFlags         _flags {};
        CompletionHandler       _completionHandler;
        CBLError                _error {};
    };
}

CBL_ASSUME_NONNULL_END
