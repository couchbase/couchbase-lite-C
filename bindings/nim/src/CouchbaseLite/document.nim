# Couchbase Lite Document class
#
# Copyright (c) 2020 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import CouchbaseLite/[database, errors, fleece]
import CouchbaseLite/private/cbl

import options
import sugar

{.experimental: "notnil".}


type Database = database.Database

type
  DocumentObj* {.requiresInit.} = object of RootObj
    handle: CBLDocument not nil
    db: Option[Database]
  Document* = ref DocumentObj
    ## An in-memory copy of a document.
    ## This object does not allow you to change the document;
    ## the subclass ``MutableDocument`` does.


  MutableDocumentObj* {.requiresInit.} = object of DocumentObj
  MutableDocument* = ref MutableDocumentObj
    ## An in-memory copy of a document that can be changed and then saved back
    ## to the database.

  ConcurrencyControl* {.pure.} =
    enum LastWriteWins = 0, FailOnConflict
    ## Conflict-handling options when saving or deleting a document.

  SaveConflictHandler* =
    proc (documentBeingSaved: MutableDocument;
        conflictingDocument: Document): bool
        ## Custom conflict handler for use when saving or deleting a document.
        ## This handler is called if the save would cause a conflict, i.e. if
        ## the document in the database has been updated (probably by a pull
        ## replicator, or by application code on another thread) since it was
        ## loaded into the Document being saved.

  Timestamp = CBLTimestamp
  ## A moment in time, used by the document expiration API.
  ## Interpreted as milliseconds since the Unix epoch (Jan 1 1970.)

proc `=destroy`(d: var DocumentObj) =
  release(d.handle)

proc `=`(dst: var DocumentObj; src: DocumentObj) {.error.} =
  echo "(can't copy a Document)"

proc mustBeInDatabase(doc: Document; db: Database) =
  if doc.db != some(db): throw(ErrorCode.InvalidParameter)

#%%% Database's document-related methods:

proc getDocument*(db: Database; docID: string): Document =
  ## Reads a document from the database, creating a new (immutable) Document
  ## object. If you are reading the document in order to make changes to it,
  ## call ``getMutableDocument`` instead.
  let doc = cbl.getDocument(db.internal_handle, docID)
  if doc != nil:
    return Document(handle: doc, db: some(db))

proc `[]`*(db: Database; docID: string): Document = db.getDocument(docID)

proc getMutableDocument*(db: Database; docID: string): MutableDocument =
  ## Reads a document from the database, in mutable form that can be updated
  ## and saved.
  let doc = getMutableDocument(db.internal_handle, docID)
  if doc != nil:
    return MutableDocument(handle: doc, db: some(db))

type SaveContext = tuple [doc: MutableDocument; handler: SaveConflictHandler]

proc saveCallback(context: pointer; documentBeingSaved,
    conflictingDocument: CBLDocument): bool =
  let ctx = (cast[ptr SaveContext](context))[]
  var conflicting: Document = nil
  if conflictingDocument != nil:
    conflicting = Document(handle: conflictingDocument, db: ctx.doc.db)
  return ctx.handler(ctx.doc, conflicting)

proc saveDocument*(db: Database; doc: MutableDocument;
    concurrency: ConcurrencyControl = LastWriteWins): Document {.discardable.} =
  ## Saves a (mutable) document to the database.
  ## If a conflicting revision has been saved since the document was loaded,
  ## the ``concurrency`` parameter specifies whether the save should fail, or
  ## the conflicting revision should be overwritten with the revision being
  ## saved. If you need finer-grained control, call the version of this method
  ## that takes a ``SaveConflictHandler`` instead.
  var err: CBLError
  let newDoc = db.internal_handle.saveDocument(doc.handle, cast[
      CBLConcurrencyControl](concurrency), err)
  if newDoc == nil:
    raise mkError(err)
  else:
    return Document(handle: newDoc, db: some(db))

proc saveDocument*(db: Database; doc: MutableDocument;
    handler: SaveConflictHandler): Document {.discardable.} =
  ## Saves a (mutable) document to the database.
  ## If a conflicting revision has been saved since the document was loaded,
  ## the callback is called with (a) the document being saved, and (b) the
  ## conflicting version already in the database (which may be nil, if the
  ## conflict is that the document was deleted.) The callback should make the
  ## appropriate changes to the document and return ``true``, or give up and
  ## return ``false``, in which case a conflict exception will be thrown.
  var context: SaveContext = (doc, handler)
  var err: CBLError
  let newDoc = db.internal_handle.saveDocumentResolving(doc.handle,
      saveCallback, addr context, err)
  if newDoc == nil:
    throw(err)
  else:
    return Document(handle: newDoc, db: some(db))

proc deleteDocument*(db: Database; doc: Document;
    concurrency: ConcurrencyControl = LastWriteWins) {.discardable.} =
  ## Deletes a document from the database. Deletions are replicated, so
  ## document metadata will be left behind that can later be pushed by the
  ## replicator.
  doc.mustBeInDatabase(db)
  checkBool( (err) => doc.handle.delete(cast[CBLConcurrencyControl](
      concurrency), err[]))

proc purgeDocument*(db: Database; doc: Document) =
  ## Purges a document, removing all traces of the document from the database.
  ## Purges are *not* replicated. If the document is changed on a server, it
  ## will be re-created when pulled.
  doc.mustBeInDatabase(db)
  checkBool( (err) => doc.handle.purge(err[]))

proc purgeDocument*(db: Database; docID: string): bool =
  ## Purges a document, removing all traces of the document from the database.
  ## Purges are *not* replicated. If the document is changed on a server, it
  ## will be re-created when pulled. If no document with this ID exists,
  ## returns false.
  var err: CBLError
  if db.internal_handle.purgeDocumentByID(docID, err):
    return true
  elif err.code == 0:
    return false
  else:
    throw(err)

proc getDocumentExpiration*(db: Database; docID: string): Timestamp =
  ## Returns the time, if any, at which a given document will expire and be
  ## purged. No expiration is indicated by a zero return value. Documents don't
  ## normally expire; you have to set an expiration time yourself.
  var err: CBLError
  let t = db.internal_handle.getDocumentExpiration(docID, err)
  if t < 0: throw(err)
  return t

proc setDocumentExpiration*(db: Database; docID: string;
    expiration: Timestamp) =
  ## Sets a document's expiration time, or clears it if the timestamp is zero.
  checkBool( (err) => db.internal_handle.setDocumentExpiration(docID,
      expiration, err[]))


#%%% Document accessors:

proc id*(doc: Document): string =
  ## The document's ID in the database.
  $(doc.handle.id)

proc revisionID*(doc: Document): string =
  ## The document's revision ID, which is a short opaque string that's
  ## guaranteed to be unique to every change made to the document. If the
  ## document doesn't exist yet, returns an empty string.
  $(doc.handle.revisionID)

proc sequence*(doc: Document): uint64 =
  ## Returns a document's current *sequence* in the local database.
  ## This number increases every time the document is saved, and a more
  ## recently saved document will have a greater sequence number than one saved
  ## earlier, so sequences may be used as an abstract 'clock' to tell relative
  ## local modification times.
  doc.handle.sequence


#%%% Document properties:

proc properties*(doc: Document): Dict =
  ## The document's properties, a key/value dictionary.
  ## This object is immutable; the subclass ``MutableDocument`` returns a
  ## mutable Dict.
  doc.handle.properties

proc `[]`*(doc: Document; key: string): Value =
  ## Returns the value of a property. Equivalent to ``doc.properties[key]``.
  doc.properties[key]

proc `[]`*(doc: Document; key: var DictKey): Value =
  ## Returns the value of a property. Equivalent to ``doc.properties[key]``.
  doc.properties[key]

proc propertiesAsJSON*(doc: Document): cstring =
  ## Returns a document's properties as a JSON string.
  ## This can be useful for interoperating with other APIs that can operate on
  ## JSON, but be aware that the extra encoding and parsing will hurt
  ## performance; it's best to use the Fleece API directly.
  doc.handle.propertiesAsJSON


#%%% MutableDicument:

proc mkDoc(doc: CBLDocument): MutableDocument =
  if doc == nil:
    throw(CBLError(domain: CBLDomain, code: int32(
        CBLErrorCode.ErrorMemoryError)))
  else:
    assert doc.mutableProperties != nil # Make sure it's mutable
    return MutableDocument(handle: doc, db: none(Database))

proc newDocument*(): MutableDocument =
  ## Creates a new, empty document in memory. It will not be added to a
  ## database until saved. Its ID will be a randomly-generated UUID.
  mkDoc(cbl.newDocument(nil))

proc newDocument*(docID: string): MutableDocument =
  ## Creates a new, empty document in memory. It will not be added to a
  ## database until saved.
  mkDoc(cbl.newDocument(docID))

proc mutableCopy*(doc: Document): MutableDocument =
  ## Creates a new MutableDocument instance that refers to the same document as
  ## the original. If the original document is mutable and has unsaved changes,
  ## the new one will also start out with the same changes; but mutating one
  ## document thereafter will not affect the other.
  mkDoc(doc.handle.mutableCopy())

proc properties*(doc: MutableDocument): MutableDict =
  ## Returns a mutable document's properties as a mutable dictionary.
  ## You may modify this dictionary and then call ``Database.saveDocument`` to
  ## persist the changes.
  wrap(doc.handle.mutableProperties)

proc `[]=`*(doc: MutableDocument; key: string; value: Settable) =
  ## Sets the value of a property. Equivalent to ``doc.properties[key] =
  ## value``.
  doc.properties[key] = value

proc `propertiesAsJSON=`*(doc: MutableDocument; json: string) =
  ## Sets the document's properties from the JSON string, which must contain an
  ## object.
  checkBool( (err) => doc.handle.setPropertiesAsJSON(json, err[]))
