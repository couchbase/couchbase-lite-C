# Errors

import CouchbaseLite/private/cbl
import CouchbaseLite/fleece

import strformat
import sugar


type
    ## CouchbaseLiteError codes
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

    ## NetworkError codes
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

#%%%% Internal error handling utilities:

# TODO: Avoid making these public

proc mkError*(err: cbl.Error): Error =
    case err.domain:
        of CBLDomain:       CouchbaseLiteError(cblErr: err, code: CBLErrorCode(err.code))
        of POSIXDomain:     POSIXError(cblErr: err, errno: err.code)
        of SQLiteDomain:    SQLiteError(cblErr: err, code: err.code)
        of FleeceDomain:    FleeceError(cblErr: err, code: FleeceErrorCode(err.code))
        of NetworkDomain:   NetworkError(cblErr: err, code: NetworkErrorCode(err.code))
        of WebSocketDomain: WebSocketError(cblErr: err, code: err.code)
        else:               CouchbaseLiteError(cblErr: err, code: CBLErrorCode(UnexpectedError))

proc throw*(err: cbl.Error) =
    raise mkError(err)

proc checkBool*(fn: (ptr cbl.Error) -> bool) =
    var err: cbl.Error
    if not fn(addr err):
        throw(err)
