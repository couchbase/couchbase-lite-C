//
// CBLCheckpoint.h
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

#ifdef __cplusplus
extern "C" {
#endif

    /** Opaque reference to a checkpoint object. */
    typedef struct CBLCheckpoint CBLCheckpoint;

    CBL_REFCOUNTED(CBLCheckpoint*, Checkpoint);


    /** A sequence number in the local database. */
    typedef uint64_t CBLSequenceNumber;
    

    /** Creates a checkpoint object for this URL and replicator options. */
    CBLCheckpoint* CBLCheckpoint_New(const CBLReplicatorConfiguration* _cbl_nonnull,
                                     bool reset,
                                     CBLError *outError) CBLAPI;

    //---- COMPARING WITH REMOTE CHECKPOINT:

    /** Returns the doc ID to store the checkpoint in the remote database. */
    FLSlice CBLCheckpoint_GetID(CBLCheckpoint* _cbl_nonnull) CBLAPI;

    /** Compares the checkpoint state with the contents of the remote checkpoint document.
        If they don't match, the local state is reset, so the replication will start from scratch. */
    bool CBLCheckpoint_CompareWithRemote(CBLCheckpoint* _cbl_nonnull,
                                         const char *remoteJSON,
                                         CBLError*) CBLAPI;

    bool CBLCheckpoint_CompareWithRemote_s(CBLCheckpoint* _cbl_nonnull,
                                           FLSlice remoteJSON,
                                           CBLError*) CBLAPI;

    //---- LOCAL SEQUENCES (PUSH):

    /** The checkpoint's local sequence. All sequences up through this one are pushed. */
    CBLSequenceNumber CBLCheckpoint_LocalMinSequence(CBLCheckpoint* _cbl_nonnull) CBLAPI;

    /** Marks this local sequence as existing and unpushed. */
    void CBLCheckpoint_AddPendingSequence(CBLCheckpoint* _cbl_nonnull,
                                          CBLSequenceNumber) CBLAPI;

    /** Records new local sequences.
        First all sequences in the range [first..last] (inclusive) are marked as complete.
        Then the sequences in the `pendingSequences` array are marked as pending.

        For example: You query for sequences starting from 100, and you get 103, 105, 108.
        You decide 108 shouldn't be pushed. You then call
        `CBLCheckpoint_addSequences(c, 100, 108, 2, {103, 105})`. */
    void CBLCheckpoint_AddSequences(CBLCheckpoint* _cbl_nonnull,
                                    CBLSequenceNumber first,
                                    CBLSequenceNumber last,
                                    size_t numPending,
                                    const CBLSequenceNumber pendingSequences[]) CBLAPI;

    /** Returns the total number of known pending sequences. */
    size_t CBLCheckpoint_PendingSequenceCount(CBLCheckpoint* _cbl_nonnull) CBLAPI;

    /** Marks a sequence number as completed. */
    void CBLCheckpoint_CompletedSequence(CBLCheckpoint* _cbl_nonnull,
                                         CBLSequenceNumber) CBLAPI;

    /** Returns true if the given sequence number has been marked as completed. */
    bool CBLCheckpoint_IsSequenceCompleted(CBLCheckpoint* _cbl_nonnull,
                                           CBLSequenceNumber) CBLAPI;

    //---- REMOTE SEQUENCES (PULL):

    /** The checkpoint's remote sequence, the last one up to which all is pulled. */
    FLSliceResult CBLCheckpoint_RemoteMinSequence(CBLCheckpoint* _cbl_nonnull) CBLAPI;

    /** Updates the checkpoint's remote sequence. */
    void CBLCheckpoint_UpdateRemoteMinSequence(CBLCheckpoint* _cbl_nonnull,
                                               const char *sequenceID) CBLAPI;

    //---- SAVING:

    /** Callback invoked when the checkpoint should be saved to the remote database.
        The callback should start an asynchronous save operation and then return ASAP.
        When the save is complete, it must call `CBLCheckpoint_saveCompleted`. */
    typedef void (*CBLCheckpointSaveCallback)(void *context, const char *jsonToSave);

    /** Enables (auto)saving the checkpoint: at about the given duration after a change is made,
        the callback will be invoked, and passed a JSON representation of the checkpoint. */
    void CBLCheckpoint_EnableSave(CBLCheckpoint* _cbl_nonnull,
                                  int timeIntervalSecs,
                                  CBLCheckpointSaveCallback _cbl_nonnull,
                                  void *context) CBLAPI;

    /** Disables autosave. Returns true if no more calls to the save callback will be made. The only
        case where another call might be made is if a save is currently in
        progress, and the checkpoint has been changed since the save began. In that case,
        another save will have to be triggered immediately when the current one finishes. */
    void CBLCheckpoint_StopAutosave(CBLCheckpoint* _cbl_nonnull) CBLAPI;

    /** Triggers an immediate save, if necessary, by calling the save callback.
        If a save is already in progress the function returns false, but the Checkpoint remembers that
        a new save is needed and will call the checkpoint as soon as the current save completes. */
    bool CBLCheckpoint_StartSave(CBLCheckpoint* _cbl_nonnull) CBLAPI;

    /** The client should call this as soon as its save completes, which can be after the
        SaveCallback returns.
        @param checkpoint  The checkpoint.
        @param successfully  True if the save was successful, false if it failed. */
    void CBLCheckpoint_SaveCompleted(CBLCheckpoint *checkpoint _cbl_nonnull,
                                     bool successfully) CBLAPI;

    /** Returns true if the checkpoint has changes that haven't been saved yet. */
    bool CBLCheckpoint_IsUnsaved(CBLCheckpoint* _cbl_nonnull) CBLAPI;

    //---- PEER CHECKPOINTS (PASSIVE REPLICATOR):

    /** Reads a previously-stored peer replicator's checkpoint from a database.
        \note You are responsible for freeing the body and revID. */
    bool CBLDatabase_GetPeerCheckpoint(CBLDatabase* _cbl_nonnull,
                                       const char *checkpointID,
                                       FLSliceResult *outBody _cbl_nonnull,
                                       FLSliceResult *outRevID _cbl_nonnull,
                                       CBLError*) CBLAPI;

    /** Stores a peer replicator's checkpoint in a database.
        \note You are responsible for freeing the newRevID.  */
    bool CBLDatabase_SetPeerCheckpoint(CBLDatabase* _cbl_nonnull,
                                       const char *checkpointID _cbl_nonnull,
                                       const char *body _cbl_nonnull,
                                       const char *revID,
                                       FLSliceResult *outNewRevID _cbl_nonnull,
                                       CBLError*) CBLAPI;

#ifdef __cplusplus
}
#endif

