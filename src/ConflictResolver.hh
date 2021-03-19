//
// ConflictResolver.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "CBLReplicatorConfig.hh"
#include "c4Replicator.h"
#include <functional>

struct CBLDatabase;

namespace cbl_internal {
    using namespace std;
    using namespace fleece;

    /** Resolves a replication conflict in a document, synchronously or asynchronously. */
    class ConflictResolver {
    public:
        /// Basic constructor.
        ConflictResolver(CBLDatabase *db _cbl_nonnull,
                         CBLConflictResolver customResolver, void* context,
                         alloc_slice docID,
                         alloc_slice revID = nullslice);

        ConflictResolver(CBLDatabase* _cbl_nonnull,
                         CBLConflictResolver,
                         void *context,
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
        bool customResolve(CBLDocument *conflict _cbl_nonnull);

        Retained<CBLDatabase>   _db;
        CBLConflictResolver     _clientResolver;
        void*                   _clientResolverContext;
        alloc_slice const       _docID;
        alloc_slice             _revID;
        C4RevisionFlags         _flags {};
        CompletionHandler       _completionHandler;
        CBLError                _error {};
    };



    /** Scans the database for all unresolved conflicts and resolves them. */
    class AllConflictsResolver {
    public:
        explicit AllConflictsResolver(CBLDatabase* _cbl_nonnull,
                                      CBLConflictResolver,
                                      void *context);
        void runNow();

    private:
        bool next();
        
        Retained<CBLDatabase>               _db;
        CBLConflictResolver                 _clientResolver;
        void*                               _clientResolverContext;
        std::unique_ptr<C4DocEnumerator>    _enum;
    };

}
