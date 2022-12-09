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
#include "cbl/CouchbaseLite.h"


CBL_CAPI_BEGIN

    void CBLError_SetCaptureBacktraces(bool) CBLAPI;
    bool CBLError_GetCaptureBacktraces(void) CBLAPI;

    void CBLLog_BeginExpectingExceptions() CBLAPI;
    void CBLLog_EndExpectingExceptions() CBLAPI;

/** Returns the collection's database, or NULL if the collection is invalid, or the database is released. */
    CBLDatabase* _cbl_nullable CBLCollection_Database(const CBLCollection*) CBLAPI;

/** Returns the last sequence number assigned in the database (default collection).
    This starts at zero and increments every time a document is saved or deleted. */
    uint64_t CBLDatabase_LastSequence(const CBLDatabase*) CBLAPI;

/** Returns the last sequence number assigned in the collection.
    This starts at zero and increments every time a document is saved or deleted. */
    uint64_t CBLCollection_LastSequence(const CBLCollection*) CBLAPI;

/** Deletes a document from the database, given only its ID.
    @note  If no document with that ID exists, this function will return false but the error
            code will be zero.
    @param database  The database.
    @param docID  The document ID to delete.
    @param outError  On failure, the error will be written here.
    @return  True if the document was deleted, false if it doesn't exist or the deletion failed. */
    bool CBLDatabase_DeleteDocumentByID(CBLDatabase* database,
                                        FLString docID,
                                        CBLError* _cbl_nullable outError) CBLAPI;

/** Deletes a document from the collection, given only its ID.
    @note  If no document with that ID exists, this function will return false but the error
            code will be zero.
    @param collection  The collection.
    @param docID  The document ID to delete.
    @param outError  On failure, the error will be written here.
    @return  True if the document was deleted, false if it doesn't exist or the deletion failed. */
    bool CBLCollection_DeleteDocumentByID(CBLCollection* collection,
                                          FLString docID,
                                          CBLError* _cbl_nullable outError) CBLAPI;

    FLSliceResult CBLDocument_CanonicalRevisionID(const CBLDocument* doc) CBLAPI;

    unsigned CBLDocument_Generation(const CBLDocument* doc) CBLAPI;
    
    FLSlice CBLReplicator_UserAgent(CBLReplicator* repl) CBLAPI;

CBL_CAPI_END
