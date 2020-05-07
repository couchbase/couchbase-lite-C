# document.nim

import CouchbaseLite/private/cbl
import CouchbaseLite/database
import CouchbaseLite/errors
import CouchbaseLite/fleece

import sugar

type Database = database.Database

type
    DocumentObj* = object of RootObj
        handle: cbl.Document
        db: Database
    Document* = ref DocumentObj

    MutableDocumentObj* = object of DocumentObj
    MutableDocument* = ref MutableDocumentObj

    ConcurrencyControl* {.pure.} = enum LastWriteWins =0, FailOnConflict
    SaveConflictHandler* = proc (documentBeingSaved: MutableDocument;
                                 conflictingDocument: Document): bool
    Timestamp = cbl.Timestamp

proc `=destroy`(d: var DocumentObj) =
    release(d.handle)

proc `=`(dst: var DocumentObj, src: DocumentObj) {.error.} =
    echo "(can't copy a Document)"


#%%% Database's document-related methods:

proc getDocument*(db: Database; docID: string): Document =
    let doc = getDocument(db.handle, docID)
    if doc == nil: return nil
    return Document(handle: doc, db: db)

proc `[]`*(db: Database, docID: string): Document   = db.getDocument(docID)

proc getMutableDocument*(db: Database; docID: string): MutableDocument =
    let doc = getMutableDocument(db.handle, docID)
    if doc == nil: return nil
    return MutableDocument(handle: doc, db: db)

proc saveCallback(context: pointer; documentBeingSaved, conflictingDocument: cbl.Document): bool =
    let handler: SaveConflictHandler = (cast[ptr SaveConflictHandler](context))[]
    var conflicting: Document = nil
    if conflictingDocument != nil:
        conflicting = Document(handle: conflictingDocument)
    return handler(MutableDocument(handle: documentBeingSaved), conflicting)

proc saveDocument*(db: Database; doc: MutableDocument;
                   concurrency: ConcurrencyControl = LastWriteWins): MutableDocument =
    var err: cbl.Error
    let newDoc = db.handle.saveDocument(doc.handle, cast[cbl.ConcurrencyControl](concurrency), err)
    if newDoc == nil: throw(err)
    return MutableDocument(handle: newDoc, db: db)

proc saveDocument*(db: Database; doc: MutableDocument; handler: SaveConflictHandler): MutableDocument =
    var err: cbl.Error
    let newDoc = db.handle.saveDocumentResolving(doc.handle, saveCallback, unsafeAddr(handler), err)
    if newDoc == nil: throw(err)
    return MutableDocument(handle: newDoc, db: db)

proc purgeDocumentByID*(db: Database; docID: string) =
    checkBool( (err) => db.handle.purgeDocumentByID(docID, err[]) )

proc getDocumentExpiration*(db: Database; docID: string): Timestamp =
    var err: cbl.Error
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

proc newDocument*(): MutableDocument =
    MutableDocument(handle: cbl.newDocument(nil))

proc newDocument*(docID: string): MutableDocument =
    MutableDocument(handle: cbl.newDocument(docID))

proc mutableCopy*(doc: Document): MutableDocument =
    MutableDocument(handle: doc.handle.mutableCopy(), db: doc.db)

proc delete*(doc: MutableDocument; concurrency: ConcurrencyControl = LastWriteWins) =
    checkBool( (err) => doc.handle.delete(cast[cbl.ConcurrencyControl](concurrency), err[]) )

proc purge*(doc: MutableDocument) =
    checkBool( (err) => doc.handle.purge(err[]) )

proc properties*(doc: MutableDocument): MutableDict =
    wrap(doc.handle.mutableProperties)

proc `[]=`*(doc: MutableDocument, key: string, value: Settable) =
    doc.mutableProperties[key] = value
