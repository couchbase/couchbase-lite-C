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

/** Database configuration options. */
typedef struct {
    const char *directory;
#ifdef COUCHBASE_ENTERPRISE
    CBLEncryptionKey encryptionKey;
#endif
} CBLDatabaseConfiguration;


// Static methods

/** Returns true if a database with the given name exists in the given directory.
    @param name  The database name (without the ".cblite2" extension.)
    @param inDirectory  The directory containing the database. If NULL, `name` must be an
                        absolute or relative path to the database. */
bool cbl_databaseExists(const char* _cblnonnull name, const char *inDirectory);

/** Copies a database file to a new location and assigns it a new UUID.
    @param fromPath  The full filesystem path to the original database (including extension).
    @param toName  The new database name (without the ".cblite2" extension.)
    @param config  The database configuration (directory and encryption option.) */
bool cbl_copyDB(const char* _cblnonnull fromPath,
                const char* _cblnonnull toName, 
                const CBLDatabaseConfiguration* config,
                CBLError*);

/** Deletes a database file. If the database is open, an error is returned.
    @param name  The database name (without the ".cblite2" extension.)
    @param inDirectory  The directory containing the database. If NULL, `name` must be an
                        absolute or relative path to the database. */
bool cbl_deleteDB(const char _cblnonnull *name, 
                  const char *inDirectory,
                  CBLError*);


// Database

CBL_REFCOUNTED(CBLDatabase*, db);

/** Opens a database, or creates it if it doesn't exist yet.
    @param name  The database name (without the ".cblite2" extension.)
    @param config  The database configuration (directory and encryption option.) */
CBLDatabase* cbl_db_open(const char *name _cblnonnull, 
                         const CBLDatabaseConfiguration* config,
                         CBLError* error);

/** Closes an open database. */
bool cbl_db_close(CBLDatabase* _cblnonnull, CBLError*);

/** Returns the database's name. */
const char* cbl_db_name(CBLDatabase* _cblnonnull);

/** Returns the database's full filesystem path. */
const char* cbl_db_path(CBLDatabase* _cblnonnull);

/** Returns the number of documents in the database. */
uint64_t cbl_db_count(CBLDatabase* _cblnonnull);

/** Returns the last sequence number assigned in the database.
    This starts at zero and increments every time a document is saved or deleted. */
uint64_t cbl_db_lastSequence(CBLDatabase* _cblnonnull);

/** Returns the database's configuration, as given when it was opened. */
const CBLDatabaseConfiguration* cbl_db_config(CBLDatabase* _cblnonnull);

/** Compacts a database file. */
bool cbl_db_compact(CBLDatabase* _cblnonnull, CBLError*);

/** Closes and deletes a database. */
bool cbl_db_delete(CBLDatabase* _cblnonnull, CBLError*);

/** Begins a batch operation, similar to a transaction.
    @note  Multiple writes are much faster when grouped inside a single batch.
    @note  Changes will not be visible to other CBLDatabase instances on the same database until
            the batch operation ends.
    @note  Batch operations can nest. Changes are not committed until the outer batch ends. */
bool cbl_db_beginBatch(CBLDatabase* _cblnonnull, CBLError*);

/** Ends a batch operation. This MUST be called after `cbl_db_beginBatch`. */
bool cbl_db_endBatch(CBLDatabase* _cblnonnull, CBLError*);


// Listeners

/** A database change listener callback, invoked after one or more documents are changed on disk.
    @param context  An arbitrary value given when the callback was registered.
    @param db  The database that changed.
    @param docIDs  The IDs of the documents that changed, as a C array of C strings,
                    ending with a NULL. */
typedef void (*CBLDatabaseListener)(void *context,
                                    CBLDatabase* db _cblnonnull,
                                    const char **docIDs);
    // NOTE: docIDs is a C array of C strings, ending with a NULL.
                                    
/** A document change listener callback, invoked after a specific document is changed on disk.
    @param context  An arbitrary value given when the callback was registered.
    @param db  The database containing the document.
    @param docID  The document's ID. */
typedef void (*CBLDocumentListener)(void *context,
                                    CBLDatabase* db _cblnonnull,
                                    const char *docID);

/** Registers a database change listener callback.
    It can be removed by calling `cbl_listener_remove`. */
CBLListenerToken* cbl_db_addListener(CBLDatabase* _cblnonnull, 
                                     CBLDatabaseListener _cblnonnull,
                                     void *context);
                                     
/** Registers a document change listener callback.
    It can be removed by calling `cbl_listener_remove`. */
CBLListenerToken* cbl_db_addDocumentListener(CBLDatabase* _cblnonnull,
                                             const char* _cblnonnull docID,
                                             CBLDocumentListener _cblnonnull, 
                                             void *context);

#ifdef __cplusplus
}
#endif
