//
//  CBLCollection.h
//
// Copyright (c) 2021 Couchbase, Inc All rights reserved.
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
#include "CBLDocument.h"
#include "CBLQuery.h"           // for CBLValueIndex, CBLFullTextIndex
#include "fleece/FLSlice.h"

CBL_CAPI_BEGIN

/** \defgroup collection Collections
    @{
    A `CBLCollection` represents a **Collection**, a named grouping of documents in a database.
    You can think of collections as "folders" or "directories" for documents (except they don't
    nest), or as like tables in a relational database.

    Each Collection provides:
    - a namespace for documents (a "docID" is only unique within its Collection)
    - a queryable container, named in `FROM` and `JOIN` clauses.
    - a scope for indexes
    - a scope for document enumerators
    - independent sequence numbers

    Every database starts with a **default Collection**, whose name is `_default`. If the database
    was created by an earlier version of LiteCore, all existing documents will be in the default
    Collection.

    \note Pre-existing functions that refer to documents / sequences / indexes without referring to
    Collections -- such as \ref CBLDatabase_GetDocument and \ref CBLDatabase_Count --
    still exist, but implicitly operate on the default Collection.
    In other words, they behave exactly the way they used to,
    but Collection-aware code should avoid them and use the new Collection API instead.
    These functions will eventually be deprecated, then removed.
 */


/** \name Lifecycle
    @{ */


/** Returns the default collection, named "`_default`", that exists in every database. */
CBLCollection* CBLDatabase_GetDefaultCollection(CBLDatabase *db) CBLAPI;

/** Returns true if the database has a collection named `name`. */
bool CBLDatabase_HasCollection(CBLDatabase *db,
                               FLString name) CBLAPI;

/** Returns the existing collection with the given name, or NULL if it doesn't exist. */
CBLCollection* _cbl_nullable CBLDatabase_GetCollection(CBLDatabase *db,
                                                       FLString name) CBLAPI;

/** Creates and returns an empty collection with the given name,
    or returns an existing collection by that name. */
CBLCollection* CBLDatabase_CreateCollection(CBLDatabase *db,
                                            FLString name,
                                            CBLError* _cbl_nullable outError) CBLAPI;

/** Deletes the collection with the given name. */
bool CBLDatabase_DeleteCollection(CBLDatabase *db,
                                  FLString name,
                                  CBLError* _cbl_nullable outError) CBLAPI;

/** Returns the names of all existing collections, as a Fleece array of strings.
    @note  You must release the array when you're finished with it. */
FLMutableArray CBLDatabase_CollectionNames(CBLDatabase *db) CBLAPI;


/** @} */
/** \name Accessors
    @{ */


/** Returns the name of the collection. */
FLString CBLCollection_Name(CBLCollection*) CBLAPI;

/** Returns the database containing this collection. */
CBLDatabase* CBLCollection_Database(CBLCollection*) CBLAPI;

/** Returns the number of (undeleted) documents in the collection. */
uint64_t CBLCollection_Count(CBLCollection*) CBLAPI;


/** @} */
/** \name Documents
    @{ */


/** Reads a document from the collection, creating a new (immutable) \ref CBLDocument object.
    Each call to this function creates a new object (which must later be released.)
    @note  If you are reading the document in order to make changes to it, call
            \ref CBLCollection_GetMutableDocument instead.
    @param collection  The collection.
    @param docID  The ID of the document.
    @param outError  On failure, the error will be written here. (A nonexistent document is not
                    considered a failure; in that event the error code will be zero.)
    @return  A new \ref CBLDocument instance, or NULL if the doc doesn't exist or an error occurred. */
_cbl_warn_unused
const CBLDocument* CBLCollection_GetDocument(const CBLCollection* collection,
                                             FLString docID,
                                             CBLError* _cbl_nullable outError) CBLAPI;

/** Reads a document from the collection, in mutable form that can be updated and saved.
    (This function is otherwise identical to \ref CBLCollection_GetDocument.)
    @note  You must release the document when you're done with it.
    @param collection  The collection.
    @param docID  The ID of the document.
    @param outError  On failure, the error will be written here. (A nonexistent document is not
                    considered a failure; in that event the error code will be zero.)
    @return  A new \ref CBLDocument instance, or NULL if the doc doesn't exist or an error occurred. */
_cbl_warn_unused
CBLDocument* CBLCollection_GetMutableDocument(CBLCollection* collection,
                                              FLString docID,
                                              CBLError* _cbl_nullable outError) CBLAPI;

/** Saves a (mutable) document to the collection.
    \warning If a newer revision has been saved since \p doc was loaded, it will be overwritten by
            this one. This can lead to data loss! To avoid this, call
            \ref CBLCollection_SaveDocumentWithConcurrencyControl or
            \ref CBLCollection_SaveDocumentWithConflictHandler instead.
    @param collection  The collection to save to.
    @param doc  The mutable document to save.
    @param outError  On failure, the error will be written here.
    @return  True on success, false on failure. */
bool CBLCollection_SaveDocument(CBLCollection* collection,
                                CBLDocument* doc,
                                CBLError* _cbl_nullable outError) CBLAPI;

/** Saves a (mutable) document to the collection.
    If a conflicting revision has been saved since \p doc was loaded, the \p concurrency
    parameter specifies whether the save should fail, or the conflicting revision should
    be overwritten with the revision being saved.
    If you need finer-grained control, call \ref CBLCollection_SaveDocumentWithConflictHandler instead.
    @param collection  The collection to save to.
    @param doc  The mutable document to save.
    @param concurrency  Conflict-handling strategy (fail or overwrite).
    @param outError  On failure, the error will be written here.
    @return  True on success, false on failure. */
bool CBLCollection_SaveDocumentWithConcurrencyControl(CBLCollection* collection,
                                                      CBLDocument* doc,
                                                      CBLConcurrencyControl concurrency,
                                                      CBLError* _cbl_nullable outError) CBLAPI;

/** Saves a (mutable) document to the collection, allowing for custom conflict handling in the event
    that the document has been updated since \p doc was loaded.
    @param collection  The collection to save to.
    @param doc  The mutable document to save.
    @param conflictHandler  The callback to be invoked if there is a conflict.
    @param context  An arbitrary value to be passed to the \p conflictHandler.
    @param outError  On failure, the error will be written here.
    @return  True on success, false on failure. */
bool CBLCollection_SaveDocumentWithConflictHandler(CBLCollection* collection,
                                                   CBLDocument* doc,
                                                   CBLConflictHandler conflictHandler,
                                                   void* _cbl_nullable context,
                                                   CBLError* _cbl_nullable outError) CBLAPI;

/** Deletes a document from the collection. Deletions are replicated.
    @warning  You are still responsible for releasing the CBLDocument.
    @param collection  The collection containing the document.
    @param document  The document to delete.
    @param outError  On failure, the error will be written here.
    @return  True if the document was deleted, false if an error occurred. */
bool CBLCollection_DeleteDocument(CBLCollection *collection,
                                  const CBLDocument* document,
                                  CBLError* _cbl_nullable outError) CBLAPI;

/** Deletes a document from the collection. Deletions are replicated.
    @warning  You are still responsible for releasing the CBLDocument.
    @param collection  The collection containing the document.
    @param document  The document to delete.
    @param concurrency  Conflict-handling strategy.
    @param outError  On failure, the error will be written here.
    @return  True if the document was deleted, false if an error occurred. */
bool CBLCollection_DeleteDocumentWithConcurrencyControl(CBLCollection *collection,
                                                        const CBLDocument* document,
                                                        CBLConcurrencyControl concurrency,
                                                        CBLError* _cbl_nullable outError) CBLAPI;

/** Moves a document to another collection, possibly with a different ID.
    @param collection  The document's original collection.
    @param docID  The ID of the document to move.
    @param toCollection  The collection to move to.
    @param newDocID  The docID in the new collection.
    @param error Information about any error that occurred
    @return  True on success, false on failure. */
bool CBLCollection_MoveDocument(CBLCollection *collection,
                                FLString docID,
                                CBLCollection *toCollection,
                                FLString newDocID,
                                CBLError* _cbl_nullable error) CBLAPI;

/** Purges a document. This removes all traces of the document from the collection.
    Purges are _not_ replicated. If the document is changed on a server, it will be re-created
    when pulled.
    @warning  You are still responsible for releasing the \ref CBLDocument reference.
    @note If you don't have the document in memory already, \ref CBLCollection_PurgeDocumentByID is a
          simpler shortcut.
    @param collection  The collection containing the document.
    @param document  The document to delete.
    @param outError  On failure, the error will be written here.
    @return  True if the document was purged, false if it doesn't exist or the purge failed. */
bool CBLCollection_PurgeDocument(CBLCollection* collection,
                                 const CBLDocument* document,
                                 CBLError* _cbl_nullable outError) CBLAPI;

/** Purges a document, given only its ID.
    @note  If no document with that ID exists, this function will return false but the error
            code will be zero.
    @param collection  The collection.
    @param docID  The document ID to purge.
    @param outError  On failure, the error will be written here.
    @return  True if the document was purged, false if it doesn't exist or the purge failed.
 */
bool CBLCollection_PurgeDocumentByID(CBLCollection* collection,
                                     FLString docID,
                                     CBLError* _cbl_nullable outError) CBLAPI;



/** Returns the time, if any, at which a given document will expire and be purged.
    Documents don't normally expire; you have to call \ref CBLCollection_SetDocumentExpiration
    to set a document's expiration time.
    @param collection  The collection.
    @param docID  The ID of the document.
    @param outError  On failure, an error is written here.
    @return  The expiration time as a CBLTimestamp (milliseconds since Unix epoch),
             or 0 if the document does not have an expiration,
             or -1 if the call failed. */
CBLTimestamp CBLCollection_GetDocumentExpiration(CBLCollection* collection,
                                                 FLSlice docID,
                                                 CBLError* _cbl_nullable outError) CBLAPI;

/** Sets or clears the expiration time of a document.
    @param collection  The collection.
    @param docID  The ID of the document.
    @param expiration  The expiration time as a CBLTimestamp (milliseconds since Unix epoch),
                        or 0 if the document should never expire.
    @param outError  On failure, an error is written here.
    @return  True on success, false on failure. */
bool CBLCollection_SetDocumentExpiration(CBLCollection* collection,
                                         FLSlice docID,
                                         CBLTimestamp expiration,
                                         CBLError* _cbl_nullable outError) CBLAPI;

/** @} */


#pragma mark - LISTENERS:


/** \name  Collection listeners
    @{
    A collection change listener lets you detect changes made to all documents in a collection.
    (If you only want to observe specific documents, use a \ref CBLDocumentChangeListener instead.)
    @note If there are multiple \ref CBLDatabase instances on the same database file, each one's
    listeners will be notified of changes made by other database instances.
    @warning  Changes made to the database file by other processes will _not_ be notified. */

/** A collection change listener callback, invoked after one or more documents are changed on disk.
    @warning  By default, this listener may be called on arbitrary threads. If your code isn't
                    prepared for that, you may want to use \ref CBLCollection_BufferNotifications
                    so that listeners will be called in a safe context.
    @param context  An arbitrary value given when the callback was registered.
    @param collection  The collection that changed.
    @param numDocs  The number of documents that changed (size of the `docIDs` array)
    @param docIDs  The IDs of the documents that changed, as a C array of `numDocs` C strings. */
typedef void (*CBLCollectionChangeListener)(void* _cbl_nullable context,
                                            const CBLCollection* collection,
                                            unsigned numDocs,
                                            FLString docIDs[_cbl_nonnull]);

/** Registers a collection change listener callback. It will be called after one or more
    documents are changed on disk.
    @param collection  The collection to observe.
    @param listener  The callback to be invoked.
    @param context  An opaque value that will be passed to the callback.
    @return  A token to be passed to \ref CBLListener_Remove when it's time to remove the
            listener.*/
_cbl_warn_unused
CBLListenerToken* CBLCollection_AddChangeListener(const CBLCollection* collection,
                                                  CBLCollectionChangeListener listener,
                                                  void* _cbl_nullable context) CBLAPI;

/** Registers a document change listener callback. It will be called after a specific document
    is changed on disk.
    @param collection  The collection to observe.
    @param docID  The ID of the document to observe.
    @param listener  The callback to be invoked.
    @param context  An opaque value that will be passed to the callback.
    @return  A token to be passed to \ref CBLListener_Remove when it's time to remove the
            listener.*/
_cbl_warn_unused
CBLListenerToken* CBLCollection_AddDocumentChangeListener(const CBLCollection* collection,
                                                          FLString docID,
                                                          CBLDocumentChangeListener listener,
                                                          void *context) CBLAPI;
/** @} */


#pragma mark - INDEXES:


/** \name  Collection Indexes
    @{ */


 /** Creates a value index.
    Indexes are persistent.
    If an identical index with that name already exists, nothing happens (and no error is returned.)
    If a non-identical index with that name already exists, it is deleted and re-created. */
bool CBLCollection_CreateValueIndex(CBLCollection *collection,
                                    FLString name,
                                    CBLValueIndex index,
                                    CBLError* _cbl_nullable outError) CBLAPI;


/** Creates a full-text index.
    Indexes are persistent.
    If an identical index with that name already exists, nothing happens (and no error is returned.)
    If a non-identical index with that name already exists, it is deleted and re-created. */
bool CBLCollection_CreateFullTextIndex(CBLCollection *collection,
                                       FLString name,
                                       CBLFullTextIndex index,
                                       CBLError* _cbl_nullable outError) CBLAPI;

/** Deletes an index given its name. */
bool CBLCollection_DeleteIndex(CBLCollection *collection,
                               FLString name,
                               CBLError* _cbl_nullable outError) CBLAPI;

/** Returns the names of the indexes on this collection, as a Fleece array of strings.
    @note  You are responsible for releasing the returned Fleece array. */
_cbl_warn_unused
FLMutableArray CBLCollection_IndexNames(CBLCollection *collection) CBLAPI;


/** @} */
/** @} */   // end Collections group

CBL_CAPI_END
