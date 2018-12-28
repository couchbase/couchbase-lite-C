//
// CBLDatabase.h
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
#include "CBLBase.h"

#ifdef __cplusplus
extern "C" {
#endif


// Database Configuration

#ifdef COUCHBASE_ENTERPRISE
/** Encryption algorithms. */
typedef CBL_ENUM(uint32_t, CBLEncryptionAlgorithm) {
    kCBLEncryptionNone = 0,      ///< No encryption (default)
    kCBLEncryptionAES256,        ///< AES with 256-bit key
};

/** Encryption key sizes (in bytes). */
typedef CBL_ENUM(uint64_t, CBLEncryptionKeySize) {
    kCBLEncryptionKeySizeAES256 = 32,
};

/** Encryption key specified in a CBLDatabaseConfiguration. */
typedef struct CBLEncryptionKey {
    CBLEncryptionAlgorithm algorithm;
    uint8_t bytes[32];
} CBLEncryptionKey;
#endif

typedef struct {
    const char *directory;
#ifdef COUCHBASE_ENTERPRISE
    CBLEncryptionKey encryptionKey;
#endif
} CBLDatabaseConfiguration;


// Static methods

bool cbl_databaseExists(const char* _cblnonnull name, const char *inDirectory);

bool cbl_copyDB(const char* _cblnonnull fromPath,
                const char* _cblnonnull toName, 
                const CBLDatabaseConfiguration*, 
                CBLError*);

bool cbl_deleteDB(const char _cblnonnull *name, 
                  const char *inDirectory,
                  CBLError*);


// Database

CBL_REFCOUNTED(CBLDatabase*, db);

CBLDatabase* cbl_db_open(const char *name _cblnonnull, 
                         const CBLDatabaseConfiguration*, 
                         CBLError*);

bool cbl_db_close(CBLDatabase* _cblnonnull, CBLError*);

const char* cbl_db_name(CBLDatabase* _cblnonnull);
const char* cbl_db_path(CBLDatabase* _cblnonnull);
uint64_t cbl_db_count(CBLDatabase* _cblnonnull);
const CBLDatabaseConfiguration* cbl_db_config(CBLDatabase* _cblnonnull);

bool cbl_db_compact(CBLDatabase* _cblnonnull, CBLError*);

bool cbl_db_delete(CBLDatabase* _cblnonnull, CBLError*);

bool cbl_db_beginBatch(CBLDatabase* _cblnonnull, CBLError*);
bool cbl_db_endBatch(CBLDatabase* _cblnonnull, CBLError*);
    // NOTE: Without blocks/lambdas, an `inBatch` function that takes a callback would be 
    // quite awkward to use!
    // Instead, expose the begin/end functions and trust the developer to balance them.


// Listeners

typedef void (*CBLDatabaseListener)(void *context, CBLDatabase* _cblnonnull,
                                    const char **docIDs);
    // NOTE: docIDs is a C array of C strings, ending with a NULL.
                                    
typedef void (*CBLDocumentListener)(void *context, CBLDatabase* _cblnonnull, 
                                    const char *docID);

CBLListenerToken* cbl_db_addListener(CBLDatabase* _cblnonnull, 
                                     CBLDatabaseListener _cblnonnull,
                                     void *context);
                                     
CBLListenerToken* cbl_db_addDocumentListener(CBLDatabase* _cblnonnull,
                                             const char* _cblnonnull docID,
                                             CBLDocumentListener _cblnonnull, 
                                             void *context);

#ifdef __cplusplus
}
#endif
