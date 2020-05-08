# document.nim

import CouchbaseLite/private/cbl
import CouchbaseLite/database
import CouchbaseLite/errors
import CouchbaseLite/fleece

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
                   concurrency: ConcurrencyControl = LastWriteWins): MutableDocument =
    var err: CBLError
    let newDoc = db.handle.saveDocument(doc.handle, cast[CBLConcurrencyControl](concurrency), err)
    if newDoc == nil:
        raise mkError(err)
    else:
        return MutableDocument(handle: newDoc, db: some(db))

proc saveDocument*(db: Database; doc: MutableDocument; handler: SaveConflictHandler): MutableDocument =
    var context: SaveContext = (doc, handler)
    var err: CBLError
    let newDoc = db.handle.saveDocumentResolving(doc.handle, saveCallback, addr context, err)
    if newDoc == nil:
        throw(err)
    else:
        return MutableDocument(handle: newDoc, db: some(db))

proc purgeDocumentByID*(db: Database; docID: string) =
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

proc propertiesAsJSON*(doc: Document): cstring  = $(doc.handle.propertiesAsJSON)

proc `propertiesAsJSON=`*(doc: MutableDocument; json: string) =
    checkBool( (err) => doc.handle.setPropertiesAsJSON(json, err[]) )


#%%% MutableDicument:

proc mkDoc(doc: CBLDocument): MutableDocument =
    if doc == nil:
        throw(CBLError(domain: CBLDomain, code: int32(CBLErrorCode.ErrorMemoryError)))
    else:
        return MutableDocument(handle: doc, db: none(Database))

proc newDocument*(): MutableDocument =
    mkDoc(cbl.newDocument(nil))

proc newDocument*(docID: string): MutableDocument =
    mkDoc(cbl.newDocument(docID))

proc mutableCopy*(doc: Document): MutableDocument =
    mkDoc(doc.handle.mutableCopy())

proc delete*(doc: MutableDocument; concurrency: ConcurrencyControl = LastWriteWins) =
    checkBool( (err) => doc.handle.delete(cast[CBLConcurrencyControl](concurrency), err[]) )

proc purge*(doc: MutableDocument) =
    checkBool( (err) => doc.handle.purge(err[]) )

proc properties*(doc: MutableDocument): MutableDict =
    wrap(doc.handle.mutableProperties)

proc `[]=`*(doc: MutableDocument, key: string, value: Settable) =
    doc.mutableProperties[key] = value
