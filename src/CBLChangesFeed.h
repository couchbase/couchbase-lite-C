//
// CBLChangesFeed.h
//
// Copyright (c) 2020 Couchbase, Inc All rights reserved.
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

#pragma once

#include "CBLReplicator.h"
#include "CBLCheckpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

    //---- Setup

    /// An object that lets you access the database's documents in sequence order, for purposes of
    /// custom replication/sync implementations.
    typedef struct CBLChangesFeed CBLChangesFeed;

    CBL_REFCOUNTED(CBLChangesFeed*, ChangesFeed);

    /// Configuration options for a CBLChangesFeed.
    typedef CBL_OPTIONS(uint32_t, CBLChangesFeedOptions) {
        kCBLChangesFeed_SkipDeletedDocs = 1,    ///< Ignore deletion 'tombstones' until caught up
    };

    /// Metadata of a document revision, returned from CBLChangesFeed_Next.
    typedef struct CBLChangesFeedRevision {
        FLHeapSlice         docID;          ///< The document ID
        FLHeapSlice         revID;          ///< The revision ID
        CBLDocumentFlags    flags;          ///< Indicates whether revision is a deletion
        CBLSequenceNumber   sequence;       ///< The sequence number
        uint64_t            bodySize;       ///< Estimated body size in bytes
    } CBLChangesFeedRevision;

    /// A list of document revisions, ordered by sequence, returned from CBLChangesFeed_Next.
    typedef struct CBLChangesFeedRevisions {
        CBLSequenceNumber             firstSequence;  ///< First sequence checked
        CBLSequenceNumber             lastSequence;   ///< Last sequence checked
        size_t                        count;          ///< Number of items in `revisions` array
        const CBLChangesFeedRevision* revisions[1];   ///< Each rev; array dimension is actually `count`
    } CBLChangesFeedRevisions;


    //---- Creating & Configuring

    /// Creates a CBLChangesFeed that will start after the sequence `since`. (I.e. at `since + 1`.)
    CBLChangesFeed* CBLChangesFeed_NewSince(CBLDatabase* _cbl_nonnull,
                                            CBLSequenceNumber since,
                                            CBLChangesFeedOptions options);

    /// Creates a CBLChangesFeed that will start after the checkpoint's LocalMinSequence.
    CBLChangesFeed* CBLChangesFeed_NewWithCheckpoint(CBLDatabase* _cbl_nonnull,
                                                     CBLCheckpoint* _cbl_nonnull,
                                                     CBLChangesFeedOptions options);

    /// Limits the feed to the given set of document IDs.
    void CBLChangesFeed_FilterToDocIDs(CBLChangesFeed* _cbl_nonnull,
                                       FLArray docIDs);

    /// Limits the feed to documents that pass the given filter function.
    /// \warning This may only be called before the first call to \ref CBLChangesFeed_Next.
    void CBLChangesFeed_SetFilterFunction(CBLChangesFeed* _cbl_nonnull,
                                          CBLReplicationFilter filter,
                                          void *context);

    //---- Listener

    /// Callback notifying that \ref CBLChangesFeed_Next has new revisions to return.
    typedef void (*CBLChangesFeedListener)(void *context,
                                           CBLChangesFeed* _cbl_nonnull);

    /// Adds a listener callback that will be invoked after new changes are made to the database.
    /// It will not be called until all pre-existing changes have been returned by \ref CBLChangesFeed_Next.
    /// Once called, it will not be called again until those new changes have been read.
    /// \note Like other callbacks, this is by default called on an arbitrary background thread,
    ///      unless you have previously called \ref CBLDatabase_BufferNotifications.
    /// \warning This may only be called before the first call to \ref CBLChangesFeed_Next.
    CBLListenerToken* CBLChangesFeed_AddListener(CBLChangesFeed* _cbl_nonnull,
                                                 CBLChangesFeedListener listener,
                                                 void *context);

    //---- Getting Changes

    /// Returns the latest sequence number the changes feed has examined.
    /// It may be greater than the latest sequence returned by \ref CBLChangesFeed_Next, since some
    /// sequences' revisions are filtered out or don't exist anymore.
    ///
    /// This is the number you would store persistently to pass to \ref CBLChangesFeed_NewSince
    /// the next time you get changes.
    CBLSequenceNumber CBLChangesFeed_GetLastSequenceChecked(CBLChangesFeed* _cbl_nonnull);

    /// Returns true after all pre-existing changes have been returned.
    /// This means \ref CBLChangesFeed_Next will not return any more items until the database changes.
    bool CBLChangesFeed_CaughtUp(CBLChangesFeed* _cbl_nonnull);

    /// Returns up to `limit` changes since the last sequence, or NULL if there are none.
    /// \note You must call `CBLChangesFeedRevisions_Free` when done with the returned items.
    CBLChangesFeedRevisions* CBLChangesFeed_Next(CBLChangesFeed* _cbl_nonnull,
                                                 unsigned limit);

    /// Frees the memory allocated by a CBLChangesFeedRevisions.
    void CBLChangesFeedRevisions_Free(CBLChangesFeedRevisions*);

#ifdef __cplusplus
}
#endif

