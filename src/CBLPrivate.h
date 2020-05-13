//
// CBLPrivate.h
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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
#include "CouchbaseLite.h"

#ifdef __cplusplus
extern "C" {
#endif


void CBLLog_SetConsoleLevelOfDomain(CBLLogDomain domain, CBLLogLevel level) CBLAPI;

CBLLogLevel CBLLog_ConsoleLevelOfDomain(CBLLogDomain domain) CBLAPI;

/** Returns the last sequence number assigned in the database.
    This starts at zero and increments every time a document is saved or deleted. */
uint64_t CBLDatabase_LastSequence(const CBLDatabase* _cbl_nonnull) CBLAPI;

/** Deletes a document from the database, given only its ID.
    @note  If no document with that ID exists, this function will return false but the error
            code will be zero.
    @param database  The database.
    @param docID  The document ID to delete.
    @param error  On failure, the error will be written here.
    @return  True if the document was deleted, false if it doesn't exist or the deletion failed. */
    bool CBLDatabase_DeleteDocumentByID(CBLDatabase* database _cbl_nonnull,
                                        const char* docID _cbl_nonnull,
                                        CBLError* error) CBLAPI;

    /** A more detailed look at a specific database change. */
    typedef struct {
        FLHeapSlice docID;          ///< The document's ID
        FLHeapSlice revID;          ///< The latest revision ID (or null if doc was purged)
        uint64_t sequence;          ///< The latest sequence number (or 0 if doc was purged)
        uint32_t bodySize;          ///< The size of the revision body in bytes
    } CBLDatabaseChange;    // Note: This must remain identical in layout to C4DatabaseChange

    typedef void (*CBLDatabaseChangeDetailListener)(void *context,
                                                    const CBLDatabase* db _cbl_nonnull,
                                                    unsigned numDocs,
                                                    const CBLDatabaseChange docs[] _cbl_nonnull);

    /** Registers a listener that gets a detailed look at each database change. */
    CBLListenerToken* CBLDatabase_AddChangeDetailListener(const CBLDatabase* db _cbl_nonnull,
                                            CBLDatabaseChangeDetailListener listener _cbl_nonnull,
                                            void *context) CBLAPI;

    /** Given a list of (docID, revID) pairs, finds which ones are new to this database, i.e. don't
        currently exist and are not older than what currently exists.
        @param db  The database.
        @param numRevisions  The number of document revisions to check.
        @param docIDs  An array of `numRevisions` document IDs.
        @param revIDs  An array of `numRevisions` revision IDs, matching up with `docIDs`.
        @param outIsNew  An array of `numRevisions` bools, which will be filled in with a `true` for
                each revision that's new, or `false` for ones that aren't new.
        @param outError  On failure, an error will be stored here.
        @return  True on success, false on failure. */
    bool CBLDatabase_FindNewRevisions(const CBLDatabase* db _cbl_nonnull,
                                      unsigned numRevisions,
                                      const FLSlice docIDs[],
                                      const FLSlice revIDs[],
                                      bool outIsNew[],
                                      CBLError *outError);
#ifdef __cplusplus
}
#endif
