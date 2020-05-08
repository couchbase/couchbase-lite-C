# document.nim

import CouchbaseLite/[database, errors, fleece]
import CouchbaseLite/private/cbl

import options
import sugar

{.experimental: "notnil".}


type Database = database.Database

type
    DocumentObj* = object of RootObj
        handle: CBLDocument not nil
        db: Option[Database]
    Document* = ref DocumentObj

    MutableDocumentObj* = object of DocumentObj
    MutableDocument* = ref MutableDocumentObj

    ConcurrencyControl* {.pure.} = enum LastWriteWins =0, FailOnConflict
    SaveConflictHandler* = proc (documentBeingSaved: MutableDocument;
                                 conflictingDocument: Document): bool
    Timestamp = CBLTimestamp

proc `=destroy`(d: var DocumentObj) =
    release(d.handle)

proc `=`(dst: var DocumentObj, src: DocumentObj) {.error.} =
    echo "(can't copy a Document)"

proc mustBeInDatabase(doc: Document, db: Database) =
    if doc.db != some(db): throw(ErrorCode.InvalidParameter)

#%%% Database's document-related methods:

proc getDocument*(db: Database; docID: string): Document =
    let doc = cbl.getDocument(db.handle, docID)
    if doc != nil:
        return Document(handle: doc, db: some(db))

proc `[]`*(db: Database, docID: string): Document   = db.getDocument(docID)

proc getMutableDocument*(db: Database; docID: string): MutableDocument =
    let doc = getMutableDocument(db.handle, docID)
    if doc != nil:
        return MutableDocument(handle: doc, db: some(db))

type SaveContext = tuple [doc: MutableDocument, handler: SaveConflictHandler]

proc saveCallback(context: pointer; documentBeingSaved, conflictingDocument: CBLDocument): bool =
    let ctx = (cast[ptr SaveContext](context))[]
    var conflicting: Document = nil
    if conflictingDocument != nil:
        conflicting = Document(handle: conflictingDocument, db: ctx.doc.db)
    return ctx.handler(ctx.doc, conflicting)

proc saveDocument*(db: Database; doc: MutableDocument;
                   concurrency: ConcurrencyControl = LastWriteWins): Document {.discardable.} =
    var err: CBLError
    let newDoc = db.handle.saveDocument(doc.handle, cast[CBLConcurrencyControl](concurrency), err)
    if newDoc == nil:
        raise mkError(err)
    else:
        return Document(handle: newDoc, db: some(db))

proc saveDocument*(db: Database; doc: MutableDocument; handler: SaveConflictHandler): Document {.discardable.} =
    var context: SaveContext = (doc, handler)
    var err: CBLError
    let newDoc = db.handle.saveDocumentResolving(doc.handle, saveCallback, addr context, err)
    if newDoc == nil:
        throw(err)
    else:
        return Document(handle: newDoc, db: some(db))

proc deleteDocument*(db: Database; doc: Document; concurrency: ConcurrencyControl = LastWriteWins) {.discardable.} =
    doc.mustBeInDatabase(db)
    checkBool( (err) => doc.handle.delete(cast[CBLConcurrencyControl](concurrency), err[]) )

proc purgeDocument*(db: Database; doc: Document) =
    doc.mustBeInDatabase(db)
    checkBool( (err) => doc.handle.purge(err[]) )

proc purgeDocument*(db: Database; docID: string) =
    checkBool( (err) => db.handle.purgeDocumentByID(docID, err[]) )

proc getDocumentExpiration*(db: Database; docID: string): Timestamp =
    var err: CBLError
    let t = db.handle.getDocumentExpiration(docID, err)
    if t < 0: throw(err)
    return t

proc setDocumentExpiration*(db: Database; docID: string, expiration: Timestamp) =
    checkBool( (err) => db.handle.setDocumentExpiration(docID, expiration, err[]) )


#%%% Document accessors:

proc id*(doc: Document): string = $(doc.handle.id)

proc revisionID*(doc: Document): string = $(doc.handle.revisionID)

proc sequence*(doc: Document): uint64 = doc.handle.sequence


#%%% Document properties:

proc properties*(doc: Document): Dict           = doc.handle.properties

proc `[]`*(doc: Document, key: string): Value   = doc.properties[key]
proc `[]`*(doc: Document, key: var DictKey): Value  = doc.properties[key]

proc propertiesAsJSON*(doc: Document): cstring  = $(doc.handle.propertiesAsJSON)

proc `propertiesAsJSON=`*(doc: MutableDocument; json: string) =
    checkBool( (err) => doc.handle.setPropertiesAsJSON(json, err[]) )


#%%% MutableDicument:

proc mkDoc(doc: CBLDocument): MutableDocument =
    if doc == nil:
        throw(CBLError(domain: CBLDomain, code: int32(CBLErrorCode.ErrorMemoryError)))
    else:
        assert doc.mutableProperties != nil # Make sure it's mutable
        return MutableDocument(handle: doc, db: none(Database))

proc newDocument*(): MutableDocument =
    mkDoc(cbl.newDocument(nil))

proc newDocument*(docID: string): MutableDocument =
    mkDoc(cbl.newDocument(docID))

proc mutableCopy*(doc: Document): MutableDocument =
    mkDoc(doc.handle.mutableCopy())

proc properties*(doc: MutableDocument): MutableDict =
    wrap(doc.handle.mutableProperties)

proc `[]=`*(doc: MutableDocument, key: string, value: Settable) =
    doc.properties[key] = value
