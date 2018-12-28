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


CBL_REFCOUNTED(const CBLDocument*, doc);

/** Conflict-handling options when saving or deleting a document. */
typedef CBL_ENUM(uint8_t, CBLConcurrencyControl) {
    /** The current save/delete will overwrite a conflicting revision if there is a conflict. */
    kCBLConcurrencyControlLastWriteWins,
    /** The current save/delete will fail if there is a conflict. */
    kCBLConcurrencyControlFailOnConflict
};

/** Reads a document from the database. */
const CBLDocument* cbl_db_getDocument(CBLDatabase* _cblnonnull,
                                      const char* _cblnonnull docID);

/** Saves a (mutable) document to the database.
    @param db  The database to save to.
    @param doc  The mutable document to save.
    @param concurrency  Conflict-handling strategy.
    @param error  On failure, the error will be written here.
    @return  An updated document reflecting the saved changes. */
const CBLDocument* cbl_db_saveDocument(CBLDatabase* db _cblnonnull,
                                       CBLDocument* doc _cblnonnull,
                                       CBLConcurrencyControl concurrency,
                                       CBLError* error);

/** Deletes a document given only its ID. */
bool cbl_db_deleteDocument(CBLDatabase* _cblnonnull,
                           const char* docID _cblnonnull,
                           CBLError*);

/** Purges a document given only its ID. */
bool cbl_db_purgeDocument(CBLDatabase* _cblnonnull,
                          const char* docID _cblnonnull,
                          CBLError*);

/** Deletes a document. Deletes are replicated.
    @note If you don't have the document in memory already, use `cbl_db_deleteDocumentID` instead. */
    bool cbl_doc_delete(const CBLDocument* _cblnonnull,
                        CBLConcurrencyControl,
                        CBLError*);

/** Purges a document. This removes all traces of the document from the database.
    Purges are not replicated. If the document is changed on a server, it will be re-created
    when pulled.
    @note If you don't have the document in memory already, use `cbl_db_purgeDocumentID` instead. */
bool cbl_doc_purge(const CBLDocument* _cblnonnull,
                   CBLError*);


// Mutable documents

/** Reads a document from the database, in mutable form that can be updated and saved. */
CBLDocument* cbl_db_getMutableDocument(CBLDatabase* _cblnonnull,
                                       const char* _cblnonnull docID);

/** Creates a new, empty document in memory. It will not be added to a database until saved.
    @param docID  The ID of the new document. It must be unique in the database.
                    You may pass NULL, and a new unique ID will be generated.
    @return  The document instance. */
CBLDocument* cbl_doc_new(const char *docID);

/** Creates a new CBLDocument in memory that refers to the same document as the original.
    If the original document is mutable, the new one will also have any in-memory changes to its
    properties. */
CBLDocument* cbl_doc_mutableCopy(const CBLDocument* original _cblnonnull);


// Properties & Metadata

/** Returns a document's ID. */
const char* _cblnonnull cbl_doc_id(const CBLDocument* _cblnonnull);

/** Returns a document's current sequence in the local database. */
uint64_t cbl_doc_sequence(const CBLDocument* _cblnonnull);

/** Returns a document's properties as an immutable dictionary. */
FLDict cbl_doc_properties(const CBLDocument* _cblnonnull);

/** Returns a mutable document's properties as a mutable dictionary.
    You may modify this dictionary and then call `cbl_db_saveDocument` to save the changes. */
FLMutableDict cbl_doc_mutableProperties(CBLDocument* _cblnonnull);

#ifdef __cplusplus
}
#endif
