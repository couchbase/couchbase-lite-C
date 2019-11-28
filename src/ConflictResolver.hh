//
// ConflictResolver.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "CBLUsings.hh"
#include "CBLReplicatorConfig.hh"
#include "c4Replicator.h"
#include <functional>

struct CBLDatabase;

namespace cbl_internal {

    /** Resolves a replication conflict in a document, synchronously or asynchronously. */
    class ConflictResolver {
    public:
        /// Basic constructor.
        ConflictResolver(CBLDatabase *db _cbl_nonnull,
                         CBLConflictResolver customResolver, void* context,
                         alloc_slice docID,
                         alloc_slice revID = fleece::nullslice);

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
        void _runAsyncNow() noexcept;
        bool customResolve(CBLDocument *conflict _cbl_nonnull);
        void errorFromException(const std::exception*, const string &what);

        Retained<CBLDatabase>   _db;
        CBLConflictResolver     _clientResolver;
        void*                   _clientResolverContext;
        string const            _docID;
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
        alloc_slice next(C4DocumentEnded &doc, C4Error *c4err);

        Retained<CBLDatabase>               _db;
        CBLConflictResolver                 _clientResolver;
        void*                               _clientResolverContext;
        c4::ref<C4DocEnumerator>            _enum;
    };

}
