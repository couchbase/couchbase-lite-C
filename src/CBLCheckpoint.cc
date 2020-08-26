//
// CBLCheckpoint.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
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

#include "CBLCheckpoint.h"
#include "CBLCheckpoint_Internal.hh"
#include "c4Database.h"
#include <algorithm>

CBLCheckpoint* CBLCheckpoint_New(const CBLReplicatorConfiguration *config,
                                 bool reset,
                                 CBLError *outError) CBLAPI
{
    cbl_internal::ReplicatorConfiguration conf(*config);
    CBLURLEndpoint placeholderEndpoint("ws://localhost/db");
    conf.endpoint = &placeholderEndpoint;   // Prevent conf.validate from barfing on caller's URL
    if (!conf.validate(outError))
        return nullptr;
    
    C4ReplicatorParameters params = { };
    auto type = conf.continuous ? kC4Continuous : kC4OneShot;
    if (conf.replicatorType != kCBLReplicatorTypePull)
        params.push = type;
    if (conf.replicatorType != kCBLReplicatorTypePush)
        params.pull = type;

    Encoder enc;
    enc.beginDict();
    conf.writeOptions(enc);
    enc.endDict();
    alloc_slice options = enc.finish();
    params.optionsDictFleece = options;

    alloc_slice url(c4address_toURL(config->endpoint->remoteAddress()));

    Retained<CBLCheckpoint> c = new CBLCheckpoint(config->database, params, url);
    C4Error c4err;
    bool ok = config->database->use<bool>([&](C4Database *c4db) {
        return c->read(c4db, reset, &c4err);
    });
    if (outError)
        *outError = external(c4err);
    if (!ok && c4err.code != 0)
        return nullptr;

    return retain(c);
}


FLSlice CBLCheckpoint_GetID(CBLCheckpoint* c) CBLAPI {
    return c->checkpointID();
}


bool CBLCheckpoint_CompareWithRemote(CBLCheckpoint* c,
                                     const char *remoteJSON,
                                     CBLError *outError) CBLAPI
{
    return CBLCheckpoint_CompareWithRemote_s(c, slice(remoteJSON), outError);
}


bool CBLCheckpoint_CompareWithRemote_s(CBLCheckpoint* c,
                                       FLSlice remoteJSON,
                                       CBLError *outError) CBLAPI
{
    return c->validateWith(repl::Checkpoint(remoteJSON));
}


CBLSequenceNumber CBLCheckpoint_LocalMinSequence(CBLCheckpoint* c) CBLAPI {
    return c->localMinSequence();
}


void CBLCheckpoint_AddPendingSequence(CBLCheckpoint* c, CBLSequenceNumber seq) CBLAPI {
    c->addPendingSequence(seq);
}


void CBLCheckpoint_AddSequences(CBLCheckpoint* c,
                                CBLSequenceNumber first,
                                CBLSequenceNumber last,
                                size_t numPending,
                                const CBLSequenceNumber pendingSequences[]) CBLAPI
{
    std::vector<CBLSequenceNumber> seqVec(&pendingSequences[0], &pendingSequences[numPending]);
    c->addPendingSequences(seqVec, first, last);
}


size_t CBLCheckpoint_PendingSequenceCount(CBLCheckpoint* c) CBLAPI {
    return c->pendingSequenceCount();
}


void CBLCheckpoint_CompletedSequence(CBLCheckpoint* c, CBLSequenceNumber seq) CBLAPI {
    c->completedSequence(seq);
}



bool CBLCheckpoint_IsSequenceCompleted(CBLCheckpoint* c, CBLSequenceNumber seq) CBLAPI {
    return c->isSequenceCompleted(seq);
}


//---- REMOTE SEQUENCES (PULL):

FLSliceResult CBLCheckpoint_RemoteMinSequence(CBLCheckpoint* c) CBLAPI {
    return FLSliceResult(c->remoteMinSequence().toJSON());
}


void CBLCheckpoint_UpdateRemoteMinSequence(CBLCheckpoint* c, const char *sequenceID) CBLAPI {
    c->setRemoteMinSequence(repl::RemoteSequence(sequenceID));
}


//---- REMOTE DOCUMENT STATE

bool CBLCheckpoint_GetDocumentRemoteState(CBLCheckpoint* c,
                                          const char *docID,
                                          FLSliceResult *outState,
                                          FLSliceResult *outRevID,
                                          CBLError *outError) CBLAPI
{
    return c->database()->use<bool>([&](C4Database *c4db) {
        auto rawDoc = c4raw_get(c4db, c->stateStoreName(), slice(docID), internal(outError));
        if (!rawDoc)
            return false;
        *outState = FLSlice_Copy(rawDoc->body);
        if (outRevID)
            *outRevID = FLSlice_Copy(rawDoc->meta);
        c4raw_free(rawDoc);
        return true;
    });
}

bool CBLCheckpoint_SetDocumentRemoteState(CBLCheckpoint *c,
                                          const char *docID,
                                          const char *revID,
                                          FLSlice state,
                                          CBLError *outError) CBLAPI
{
    return c->database()->use<bool>([&](C4Database *c4db) {
        if (!c4db_beginTransaction(c4db, internal(outError)))
            return false;
        bool ok = c4raw_put(c4db, c->stateStoreName(), slice(docID), slice(revID), state,
                            internal(outError));
        return c4db_endTransaction(c4db, ok, internal(outError)) && ok;
    });
}


//---- SAVING:


void CBLCheckpoint_EnableSave(CBLCheckpoint* c,
                              int timeIntervalSecs,
                              CBLCheckpointSaveCallback callback,
                              void *context) CBLAPI
{
    c->enableSave(std::chrono::seconds(timeIntervalSecs), callback, context);
}


void CBLCheckpoint_StopAutosave(CBLCheckpoint* c) CBLAPI {
    c->disableSave();
}


bool CBLCheckpoint_StartSave(CBLCheckpoint* c) CBLAPI {
    return c->save();
}


void CBLCheckpoint_SaveCompleted(CBLCheckpoint* c, bool successfully) CBLAPI {
    if (successfully)
        c->writeLatest();
    c->saveCompleted();
}


bool CBLCheckpoint_IsUnsaved(CBLCheckpoint* c) CBLAPI {
    return c->isUnsaved();
}


//---- PEER CHECKPOINTS (PASSIVE REPLICATOR):


bool CBLDatabase_GetPeerCheckpoint(CBLDatabase* db,
                                   const char *checkpointID,
                                   FLSliceResult *outBody,
                                   FLSliceResult *outRevID,
                                   CBLError *outError) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        alloc_slice body, revID;
        if (!repl::Checkpointer::getPeerCheckpoint(c4db, checkpointID, body, revID,
                                                   internal(outError)))
            return false;
        *outBody = FLSliceResult(body);
        *outRevID = FLSliceResult(revID);
        return true;
    });
}


bool CBLDatabase_SetPeerCheckpoint(CBLDatabase* db,
                                   const char *checkpointID,
                                   const char *body,
                                   const char *revID,
                                   FLSliceResult *outNewRevID,
                                   CBLError *outError) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        alloc_slice newRevID;
        if (!repl::Checkpointer::savePeerCheckpoint(c4db, checkpointID, body, revID, newRevID,
                                                    internal(outError)))
            return false;
        *outNewRevID = FLSliceResult(newRevID);
        return true;
    });
}
