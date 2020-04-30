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

#ifdef __cplusplus
}
#endif
