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

/** \defgroup database   Database
    @{
    A CBLDatabase is both a filesystem object and a container for documents.
 */

#pragma mark - CONFIGURATION
/** \name  Database configuration
    @{ */

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

/** @} */



#pragma mark - FILE OPERATIONS
/** \name  Database file operations
    @{
    These functions operate on database files without opening them.
 */

/** Returns true if a database with the given name exists in the given directory.
    @param name  The database name (without the ".cblite2" extension.)
    @param inDirectory  The directory containing the database. If NULL, `name` must be an
                        absolute or relative path to the database. */
bool cbl_databaseExists(const char* _cbl_nonnull name, const char *inDirectory) CBLAPI;

/** Copies a database file to a new location and assigns it a new UUID.
    @param fromPath  The full filesystem path to the original database (including extension).
    @param toName  The new database name (without the ".cblite2" extension.)
    @param config  The database configuration (directory and encryption option.) */
bool cbl_copyDatabase(const char* _cbl_nonnull fromPath,
                      const char* _cbl_nonnull toName,
                      const CBLDatabaseConfiguration* config,
                      CBLError*) CBLAPI;

/** Deletes a database file. If the database is open, an error is returned.
    @param name  The database name (without the ".cblite2" extension.)
    @param inDirectory  The directory containing the database. If NULL, `name` must be an
                        absolute or relative path to the database. */
bool cbl_deleteDatabase(const char _cbl_nonnull *name, 
                        const char *inDirectory,
                        CBLError*) CBLAPI;

/** @} */



#pragma mark - LIFECYCLE
/** \name  Database lifecycle
    @{
    Opening, closing, and managing open databases.
 */

/** Opens a database, or creates it if it doesn't exist yet, returning a new CBLDatabase
    instance.
    It's OK to open the same database file multiple times. Each CBLDatabase instance is
    independent of the others (and must be separately closed and released.)
    @param name  The database name (without the ".cblite2" extension.)
    @param config  The database configuration (directory and encryption option.)
    @param error  On failure, the error will be written here.
    @return  The new database object, or NULL on failure. */
_cbl_warn_unused
CBLDatabase* cbl_db_open(const char *name _cbl_nonnull,
                         const CBLDatabaseConfiguration* config,
                         CBLError* error) CBLAPI;

/** Closes an open database. */
bool cbl_db_close(CBLDatabase*, CBLError*) CBLAPI;

CBL_REFCOUNTED(CBLDatabase*, db);

/** Closes and deletes a database. */
bool cbl_db_delete(CBLDatabase* _cbl_nonnull, CBLError*) CBLAPI;

/** Compacts a database file. */
bool cbl_db_compact(CBLDatabase* _cbl_nonnull, CBLError*) CBLAPI;

/** Begins a batch operation, similar to a transaction. You **must** later call \ref
    cbl_db_endBatch to end (commit) the batch.
    @note  Multiple writes are much faster when grouped inside a single batch.
    @note  Changes will not be visible to other CBLDatabase instances on the same database until
            the batch operation ends.
    @note  Batch operations can nest. Changes are not committed until the outer batch ends. */
bool cbl_db_beginBatch(CBLDatabase* _cbl_nonnull, CBLError*) CBLAPI;

/** Ends a batch operation. This **must** be called after \ref cbl_db_beginBatch. */
bool cbl_db_endBatch(CBLDatabase* _cbl_nonnull, CBLError*) CBLAPI;

/** @} */



#pragma mark - ACCESSORS
/** \name  Database accessors
    @{
    Getting information about a database.
 */

/** Returns the database's name. */
const char* cbl_db_name(const CBLDatabase* _cbl_nonnull) CBLAPI _cbl_returns_nonnull;

/** Returns the database's full filesystem path. */
const char* cbl_db_path(const CBLDatabase* _cbl_nonnull) CBLAPI _cbl_returns_nonnull;

/** Returns the number of documents in the database. */
uint64_t cbl_db_count(const CBLDatabase* _cbl_nonnull) CBLAPI;

/** Returns the database's configuration, as given when it was opened. */
const CBLDatabaseConfiguration cbl_db_config(const CBLDatabase* _cbl_nonnull) CBLAPI;

/** @} */



#pragma mark - LISTENERS
/** \name  Database listeners
    @{
    A database change listener lets you detect changes made to all documents in a database.
    (If you only want to observe specific documents, use a \ref CBLDocumentChangeListener instead.)
    @note If there are multiple CBLDatabase instances on the same database file, each one's
    listeners will be notified of changes made by other database instances.
 */

/** A database change listener callback, invoked after one or more documents are changed on disk.
    @param context  An arbitrary value given when the callback was registered.
    @param db  The database that changed.
    @param numDocs  The number of documents that changed (size of the docIDs array)
    @param docIDs  The IDs of the documents that changed, as a C array of `numDocs` C strings. */
    typedef void (*CBLDatabaseChangeListener)(void *context,
                                              const CBLDatabase* db _cbl_nonnull,
                                              unsigned numDocs,
                                              const char **docIDs _cbl_nonnull);

/** Registers a database change listener callback. It will be called after one or more
    documents are changed on disk.
    @param db  The database to observe.
    @param listener  The callback to be invoked.
    @param context  An opaque value that will be passed to the callback.
    @return  A token to be passed to \ref cbl_listener_remove when it's time to remove the
            listener.*/
_cbl_warn_unused
CBLListenerToken* cbl_db_addChangeListener(const CBLDatabase* db _cbl_nonnull,
                                           CBLDatabaseChangeListener listener _cbl_nonnull,
                                           void *context) CBLAPI;

/** @} */
/** @} */    // end of outer \defgroup



#pragma mark - NOTIFICATION SCHEDULING
/** \defgroup listeners   Listeners
    @{ */
/** \name  Scheduling notifications
    @{
    Applications may want control over when Couchbase Lite notifications (listener callbacks)
    happen. They may want them called on a specific thread, or at certain times during an event
    loop. This behavior may vary by database, if for instance each database is associated with a
    separate thread.

    The API calls here enable this. When notifications are "buffered" for a database, calls to
    listeners will be deferred until the application explicitly allows them. Instead, a single
    callback will be issued when the first notification becomes available; this gives the app a
    chance to schedule a time when the notifications should be sent and callbacks called.
 */

/** Callback indicating that the database (or an object belonging to it) is ready to call one
    or more listeners. You should call `cbl_db_callListeners` at your earliest convenience.
    @note  This callback is called _only once_ until the next time `cbl_db_callListeners`
            is called. If you don't respond by (sooner or later) calling that function,
            you will not be informed that any listeners are ready.
    @warning  This can be called from arbitrary threads. It should do as little work as
              possible, just scheduling a future call to `cbl_db_callListeners`. */
typedef void (*CBLNotificationsReadyCallback)(void *context,
                                              CBLDatabase* db _cbl_nonnull);

/** Switches the database to buffered-notification mode. Notifications for objects belonging
    to this database will not be called immediately.
    @param db  The database whose notifications are to be buffered.
    @param callback  The function to be called when a notification is available.
    @param context  An arbitrary value that will be passed to the callback. */
void cbl_db_bufferNotifications(CBLDatabase *db _cbl_nonnull,
                                CBLNotificationsReadyCallback callback _cbl_nonnull,
                                void *context) CBLAPI;

/** Immediately issues all pending notifications for this database, by calling their listener
    callbacks. */
void cbl_db_sendNotifications(CBLDatabase *db _cbl_nonnull) CBLAPI;
                                     
/** @} */
/** @} */    // end of outer \defgroup

#ifdef __cplusplus
}
#endif
