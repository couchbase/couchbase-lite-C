//
// CBLDocument.h
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
#include "fleece/Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup documents   Documents
    @{
    A CBLDocument is essentially a JSON object with an ID string that's unique in its database.
 */

/** \name  Document lifecycle
    @{ */

/** Conflict-handling options when saving or deleting a document. */
typedef CBL_ENUM(uint8_t, CBLConcurrencyControl) {
    /** The current save/delete will overwrite a conflicting revision if there is a conflict. */
    kCBLConcurrencyControlLastWriteWins,
    /** The current save/delete will fail if there is a conflict. */
    kCBLConcurrencyControlFailOnConflict
};


/** Reads a document from the database, creating a new (immutable) CBLDocument object.
    Each call to this function creates a new object (which must later be released.)
    @note  If you are reading the document in order to make changes to it, call
            \ref cbl_db_getMutableDocument instead.
    @param database  The database.
    @param docID  The ID of the document.
    @return  A new CBLDocument instance, or NULL if no document with that ID exists. */
_cbl_warn_unused
const CBLDocument* cbl_db_getDocument(const CBLDatabase* database _cbl_nonnull,
                                      const char* _cbl_nonnull docID) CBLAPI;

CBL_REFCOUNTED(CBLDocument*, doc);

/** Saves a (mutable) document to the database.
    @param db  The database to save to.
    @param doc  The mutable document to save.
    @param concurrency  Conflict-handling strategy.
    @param error  On failure, the error will be written here.
    @return  An updated document reflecting the saved changes, or NULL on failure. */
_cbl_warn_unused
const CBLDocument* cbl_db_saveDocument(CBLDatabase* db _cbl_nonnull,
                                       CBLDocument* doc _cbl_nonnull,
                                       CBLConcurrencyControl concurrency,
                                       CBLError* error) CBLAPI;

/** Deletes a document from the database. Deletions are replicated.
    @warning  You are still responsible for releasing the CBLDocument.
    @param document  The document to delete.
    @param concurrency  Conflict-handling strategy.
    @param error  On failure, the error will be written here.
    @return  True if the document was deleted, false if an error occurred. */
bool cbl_doc_delete(const CBLDocument* document _cbl_nonnull,
                    CBLConcurrencyControl concurrency,
                    CBLError* error) CBLAPI;

/** Purges a document. This removes all traces of the document from the database.
    Purges are not replicated. If the document is changed on a server, it will be re-created
    when pulled.
    @warning  You are still responsible for releasing the CBLDocument reference.
    @note If you don't have the document in memory already, \ref cbl_db_purgeDocumentByID is a
          simpler shortcut.
    @param document  The document to delete.
    @param error  On failure, the error will be written here.
    @return  True if the document was purged, false if it doesn't exist or the purge failed. */
bool cbl_doc_purge(const CBLDocument* document _cbl_nonnull,
                   CBLError* error) CBLAPI;

/** Purges a document given only its ID.
    @note  If no document with that ID exists, this function will return false but the error
            code will be zero.
    @param database  The database.
    @param docID  The document ID to purge.
    @param error  On failure, the error will be written here.
    @return  True if the document was purged, false if it doesn't exist or the purge failed.
 */
bool cbl_db_purgeDocumentByID(CBLDatabase* database _cbl_nonnull,
                              const char* docID _cbl_nonnull,
                              CBLError* error) CBLAPI;

/** @} */



/** \name  Mutable documents
    @{
    The type `CBLDocument*` without a `const` qualifier refers to a _mutable_ document instance.
    A mutable document exposes its properties as a mutable dictionary, so you can change them
    in place and then call \ref cbl_db_saveDocument to persist the changes.
 */

/** Reads a document from the database, in mutable form that can be updated and saved.
    (This function is otherwise identical to \ref cbl_db_getDocument.)
    @note  You must release the document when you're done with it.
    @param database  The database.
    @param docID  The ID of the document.
    @return  A new mutable CBLDocument instance, or NULL if no document with that ID exists. */
_cbl_warn_unused
CBLDocument* cbl_db_getMutableDocument(CBLDatabase* database _cbl_nonnull,
                                       const char* docID _cbl_nonnull) CBLAPI;

/** Creates a new, empty document in memory. It will not be added to a database until saved.
    @param docID  The ID of the new document, or NULL to assign a new unique ID.
    @return  The mutable document instance. */
CBLDocument* cbl_doc_new(const char *docID) CBLAPI _cbl_warn_unused _cbl_returns_nonnull;

/** Creates a new CBLDocument instance that refers to the same document as the original.
    If the original document has unsaved changes, the new one will also start out with the same
    changes; but mutating one document thereafter will not affect the other.
    @note  You must release the new reference when you're done with it. */
CBLDocument* cbl_doc_mutableCopy(const CBLDocument* original _cbl_nonnull) CBLAPI
    _cbl_warn_unused _cbl_returns_nonnull;

/** @} */



/** \name  Document properties and metadata
    @{
    A document's body is essentially a JSON object. The properties are accessed in memory
    using the Fleece API, with the body itself being a \ref FLDict "dictionary").
 */

/** Returns a document's ID. */
const char* cbl_doc_id(const CBLDocument* _cbl_nonnull) CBLAPI _cbl_returns_nonnull;

/** Returns a document's current sequence in the local database.
    This number increases every time the document is saved, and a more recently saved document
    will have a greater sequence number than one saved earlier, so sequences may be used as an
    abstract 'clock' to tell relative modification times. */
uint64_t cbl_doc_sequence(const CBLDocument* _cbl_nonnull) CBLAPI;

/** Returns a document's properties as a dictionary.
    @note  The dictionary object is owned by the document; you do not need to release it.
    @warning  This dictionary _reference_ is immutable, but if the document is mutable the
           underlying dictionary itself is mutable and could be modified through a mutable
           reference obtained via \ref cbl_doc_mutableProperties. If you need to preserve the
           properties, call \ref FLDict_MutableCopy to make a deep copy. */
FLDict cbl_doc_properties(const CBLDocument* _cbl_nonnull) CBLAPI;

/** Returns a mutable document's properties as a mutable dictionary.
    You may modify this dictionary and then call \ref cbl_db_saveDocument to persist the changes.
    @note  The dictionary object is owned by the document; you do not need to release it.
    @note  Every call to this function returns the same mutable collection. This is the
           same collection returned by \ref cbl_doc_properties. */
FLMutableDict cbl_doc_mutableProperties(CBLDocument* _cbl_nonnull) CBLAPI _cbl_returns_nonnull;

/** Returns a document's properties as a null-terminated JSON string.
    @note You are responsible for calling free() on the returned string. */
char* _cbl_nonnull cbl_doc_propertiesAsJSON(const CBLDocument* _cbl_nonnull) CBLAPI;

/** Sets a mutable document's properties from a JSON string. */
bool cbl_doc_setPropertiesAsJSON(CBLDocument* _cbl_nonnull,
                                 const char *json _cbl_nonnull,
                                 CBLError*) CBLAPI;

/** Returns the time at which a given document will expire and be purged.
    @param db  The database.
    @param docID  The ID of the document.
    @param error  On failure, an error is written here.
    @return  The expiration as a standard timestamp (seconds since Unix epoch),
             or 0 if the document does not have an expiration,
             or -1 if the call failed. */
time_t cbl_db_getDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                    const char *docID _cbl_nonnull,
                                    CBLError* error);

/** Sets or clears the expiration time of a document.
    @param db  The database.
    @param docID  The ID of the document.
    @param expiration  The expiration time as a standard timestamp  (seconds since Unix epoch),
                        or 0 if the document should never expire.
    @param error  On failure, an error is written here.
    @return  True on success, false on failure. */
bool cbl_db_setDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                  const char *docID _cbl_nonnull,
                                  time_t expiration,
                                  CBLError* error);

/** @} */



/** \name  Document listeners
    @{
    A document change listener lets you detect changes made to a specific document after they
    are persisted to the database.
    @note If there are multiple CBLDatabase instances on the same database file, each one's
    document listeners will be notified of changes made by other database instances.
 */

/** A document change listener callback, invoked after a specific document is changed on disk.
    @param context  An arbitrary value given when the callback was registered.
    @param db  The database containing the document.
    @param docID  The document's ID. */
typedef void (*CBLDocumentChangeListener)(void *context,
                                          const CBLDatabase* db _cbl_nonnull,
                                          const char *docID _cbl_nonnull);

/** Registers a document change listener callback. It will be called after a specific document
    is changed on disk.
    @param db  The database to observe.
    @param docID  The ID of the document to observe.
    @param listener  The callback to be invoked.
    @param context  An opaque value that will be passed to the callback.
    @return  A token to be passed to \ref cbl_listener_remove when it's time to remove the
            listener.*/
_cbl_warn_unused
CBLListenerToken* cbl_db_addDocumentChangeListener(const CBLDatabase* db _cbl_nonnull,
                                                   const char* docID _cbl_nonnull,
                                                   CBLDocumentChangeListener listener _cbl_nonnull,
                                                   void *context) CBLAPI;

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif
