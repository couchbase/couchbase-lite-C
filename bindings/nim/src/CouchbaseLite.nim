# CouchbaseLite.nim

import cbl
import fleece

import strformat
import sugar

#################### EXCEPTIONS


type
    ## Couchbase Lite error codes, in the CBL error domain.
    CBLErrorCode* = enum
        AssertionFailed = 1, ## Internal assertion failure
        Unimplemented,       ## Oops, an unimplemented API call
        UnsupportedEncryption,## Unsupported encryption algorithm
        BadRevisionID,       ## Invalid revision ID syntax
        CorruptRevisionData, ## Revision contains corrupted/unreadable data
        NotOpen,             ## Database/KeyStore/index is not open
        NotFound,            ## Document not found
        Conflict,            ## Document update conflict
        InvalidParameter,    ## Invalid function parameter or struct value
        UnexpectedError,     ## Internal unexpected C++ exception
        CantOpenFile,        ## Database file can't be opened; may not exist
        IOError,             ## File I/O error
        MemoryError,         ## Memory allocation failed (out of memory?)
        NotWriteable,        ## File is not writeable
        CorruptData,         ## Data is corrupted
        Busy,                ## Database is busy/locked
        NotInTransaction,    ## Function must be called while in a transaction
        TransactionNotClosed,## Database can't be closed while a transaction is open
        Unsupported,         ## Operation not supported in this database
        NotADatabaseFile,    ## File is not a database, or encryption key is wrong
        WrongFormat,         ## Database exists but not in the format/storage requested
        Crypto,              ## Encryption/decryption error
        InvalidQuery,        ## Invalid query
        MissingIndex,        ## No such index, or query requires a nonexistent index
        InvalidQueryParam,   ## Unknown query param name, or param number out of range
        RemoteError,         ## Unknown error from remote server
        DatabaseTooOld,      ## Database file format is older than what I can open
        DatabaseTooNew,      ## Database file format is newer than what I can open
        BadDocID,            ## Invalid document ID
        CantUpgradeDatabase  ## DB can't be upgraded (might be unsupported dev version)

    ## Network error codes, in the Network error domain.
    NetworkErrorCode* = enum
        DNSFailure = 1,     ## DNS lookup failed
        UnknownHost,        ## DNS server doesn't know the hostname
        Timeout,            ## No response received before timeout
        InvalidURL,         ## Invalid URL
        TooManyRedirects,   ## HTTP redirect loop
        TLSHandshakeFailed, ## Low-level error establishing TLS
        TLSCertExpired,     ## Server's TLS certificate has expired
        TLSCertUntrusted,   ## Cert isn't trusted for other reason
        TLSClientCertRequired, ## Server requires client to have a TLS certificate
        TLSClientCertRejected, ## Server rejected my TLS client certificate
        TLSCertUnknownRoot, ## Self-signed cert, or unknown anchor cert
        InvalidRedirect,    ## Attempted redirect to invalid URL
        Unknown,            ## Unknown networking error
        TLSCertRevoked,     ## Server's cert has been revoked
        TLSCertNameMismatch ## Server cert's name does not match DNS name

type
    Error* = ref object of CatchableError
        cblErr: cbl.Error
    CouchbaseLiteError* = ref object of Error
        code*: CBLErrorCode
    POSIXError* = ref object of Error
        errno*: int
    SQLiteError* = ref object of Error
        code*: int
    FleeceError* = ref object of Error
        code*: FleeceErrorCode
    NetworkError* = ref object of Error
        code*: NetworkErrorCode
    WebSocketError* = ref object of Error
        code*: int

proc message*(err: Error): string = $(message(err.cblErr))
proc codeStr*(err: Error): string =
    case err.cblErr.domain:
        of CBLDomain:     $(cast[CBLErrorCode](err.cblErr.code))
        of FleeceDomain:  $(cast[FleeceErrorCode](err.cblErr.code))
        of NetworkDomain: $(cast[NetworkErrorCode](err.cblErr.code))
        else:             &"{err.cblErr.code}"

proc `$`*(err: Error): string = &"{err.message} ({err.cblErr.domain}.{err.cblErr.code}: {err.codeStr})"


## Internal error handling utilities:

proc mkError(err: cbl.Error): Error =
    case err.domain:
        of CBLDomain:       CouchbaseLiteError(cblErr: err, code: CBLErrorCode(err.code))
        of POSIXDomain:     POSIXError(cblErr: err, errno: err.code)
        of SQLiteDomain:    SQLiteError(cblErr: err, code: err.code)
        of FleeceDomain:    FleeceError(cblErr: err, code: FleeceErrorCode(err.code))
        of NetworkDomain:   NetworkError(cblErr: err, code: NetworkErrorCode(err.code))
        of WebSocketDomain: WebSocketError(cblErr: err, code: err.code)
        else:               CouchbaseLiteError(cblErr: err, code: CBLErrorCode(UnexpectedError))

proc throw(err: cbl.Error) =
    raise mkError(err)

proc checkBool(fn: (ptr cbl.Error) -> bool) =
    var err: cbl.Error
    if not fn(addr err):
        throw(err)


#################### DATABASE


type
    DatabaseObj* = object
        handle: cbl.Database
    Database* = ref DatabaseObj


proc `=destroy`(d: var DatabaseObj) =
    release(d.handle)

proc `=`(dst: var DatabaseObj, src: DatabaseObj) {.error.} =
    echo "can't copy a db"


type
    DatabaseFlag* {.size: sizeof(cint).} = enum
        create,             ## Create the file if it doesn't exist
        readOnly,           ## Open file read-only
        noUpgrade           ## Disable upgrading an older-version database
    DatabaseFlags* = set[DatabaseFlag]

    EncryptionAlgorithm* = enum
        none = 0,
        AES256
    EncryptionKey* = object
        algorithm*: EncryptionAlgorithm ## Encryption algorithm
        bytes*: array[32, uint8]        ## Raw key data

    DatabaseConfiguration* = object
        directory*: string
        flags*: DatabaseFlags
        encryptionKey*: ref EncryptionKey


proc openDB(name: string, configP: ptr cbl.DatabaseConfiguration): Database =
    var err: cbl.Error
    let dbRef = cbl.openDatabase(name, configP, err)
    if dbRef == nil:
        throw(err)
    return Database(handle: dbRef)

proc openDatabase*(name: string, config: DatabaseConfiguration): Database =
    var cblConfig = cbl.DatabaseConfiguration(
        directory: config.directory,
        flags: cast[cbl.DatabaseFlags](config.flags) )
    var cblKey: cbl.EncryptionKey
    if config.encryptionKey != nil:
        cblKey.algorithm = cast[cbl.EncryptionAlgorithm](config.encryptionKey.algorithm)
        cblKey.bytes = config.encryptionKey.bytes
        cblConfig.encryptionKey = addr cblKey
    return openDB(name, addr cblConfig)

proc openDatabase*(name: string): Database =
    return openDB(name, nil)


proc databaseExists*(name: string; inDirectory: string): bool =
    cbl.databaseExists(name, inDirectory)

proc deleteDatabase*(name: string; inDirectory: string): bool =
    var err: cbl.Error
    if cbl.deleteDatabase(name, inDirectory, err):
        return true
    elif err.code == 0:
        return false
    else:
        throw(err)

proc name*(db: Database): string = $(db.handle.name)

proc path*(db: Database): string = $(db.handle.path)

proc count*(db: Database): uint64 = db.handle.count

proc close*(db: Database) = checkBool( (err) => cbl.close(db.handle, err[]) )

proc delete*(db: Database) = checkBool( (err) => cbl.delete(db.handle, err[]) )

proc compact*(db: Database) = checkBool( (err) => cbl.compact(db.handle, err[]) )

proc inBatch*(db: Database, fn: proc()) =
    checkBool( (err) => cbl.beginBatch(db.handle, err[]) )
    defer: checkBool( (err) => cbl.endBatch(db.handle, err[]) )
    fn()

#TODO: Listeners


#################### DOCUMENT


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

#### Database's document-related methods:

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

proc saveDocument*(db: Database; doc: MutableDocument; concurrency: ConcurrencyControl = LastWriteWins): MutableDocument =
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

#### Document accessors:

proc id*(doc: Document): string = $(doc.handle.id)

proc revisionID*(doc: Document): string = $(doc.handle.revisionID)

proc sequence*(doc: Document): uint64 = doc.handle.sequence

#### Document properties:

proc properties*(doc: Document): Dict           = doc.handle.properties

proc `[]`*(doc: Document, key: string): Value   = doc.properties[key]

proc propertiesAsJSON*(doc: Document): cstring  = $(doc.handle.propertiesAsJSON)

proc `propertiesAsJSON=`*(doc: MutableDocument; json: string) =
    checkBool( (err) => doc.handle.setPropertiesAsJSON(json, err[]) )

#### MutableDicument:

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
