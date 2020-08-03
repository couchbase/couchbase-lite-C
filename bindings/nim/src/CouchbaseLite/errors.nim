# Couchbase Lite exception classes
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

import CouchbaseLite/fleece
import CouchbaseLite/private/cbl

import strformat
import sugar

{.experimental: "notnil".}


type

  ErrorCode* = enum        ## Couchbase Lite error codes, found in ``Error`` objects.
    AssertionFailed = 1,   ## Internal assertion failure
    Unimplemented,         ## Oops, an unimplemented API call
    UnsupportedEncryption, ## Unsupported encryption algorithm
    BadRevisionID,         ## Invalid revision ID syntax
    CorruptRevisionData,   ## Revision contains corrupted/unreadable data
    NotOpen,               ## Database/KeyStore/index is not open
    NotFound,              ## Document not found
    Conflict,              ## Document update conflict
    InvalidParameter,      ## Invalid function parameter or struct value
    UnexpectedError,       ## Internal unexpected C++ exception
    CantOpenFile,          ## Database file can't be opened; may not exist
    IOError,               ## File I/O error
    MemoryError,           ## Memory allocation failed (out of memory?)
    NotWriteable,          ## File is not writeable
    CorruptData,           ## Data is corrupted
    Busy,                  ## Database is busy/locked
    NotInTransaction,      ## Function must be called while in a transaction
    TransactionNotClosed,  ## Database can't be closed while a transaction is open
    Unsupported,           ## Operation not supported in this database
    NotADatabaseFile,      ## File is not a database, or encryption key is wrong
    WrongFormat,           ## Database exists but not in the format/storage requested
    Crypto,                ## Encryption/decryption error
    InvalidQuery,          ## Invalid query
    MissingIndex,          ## No such index, or query requires a nonexistent index
    InvalidQueryParam,     ## Unknown query param name, or param number out of range
    RemoteError,           ## Unknown error from remote server
    DatabaseTooOld,        ## Database file format is older than what I can open
    DatabaseTooNew,        ## Database file format is newer than what I can open
    BadDocID,              ## Invalid document ID
    CantUpgradeDatabase    ## DB can't be upgraded (might be unsupported dev version)

  NetworkErrorCode* = enum ## Network error codes, found in ``NetworkError`` objects.
    DNSFailure = 1,        ## DNS lookup failed
    UnknownHost,           ## DNS server doesn't know the hostname
    Timeout,               ## No response received before timeout
    InvalidURL,            ## Invalid URL
    TooManyRedirects,      ## HTTP redirect loop
    TLSHandshakeFailed,    ## Low-level error establishing TLS
    TLSCertExpired,        ## Server's TLS certificate has expired
    TLSCertUntrusted,      ## Cert isn't trusted for other reason
    TLSClientCertRequired, ## Server requires client to have a TLS certificate
    TLSClientCertRejected, ## Server rejected my TLS client certificate
    TLSCertUnknownRoot,    ## Self-signed cert, or unknown anchor cert
    InvalidRedirect,       ## Attempted redirect to invalid URL
    Unknown,               ## Unknown networking error
    TLSCertRevoked,        ## Server's cert has been revoked
    TLSCertNameMismatch    ## Server cert's name does not match DNS name

type
  Error* = ref object of CatchableError
    ## Base class for Couchbase Lite exceptions.
    cblErr: CBLError
  CouchbaseLiteError* = ref object of Error
    ## A Couchbase Lite-defined error. See its ``code`` for details.
    code*: ErrorCode
  POSIXError* = ref object of Error
    ## An OS error. See the ``errno`` property for the specific error (defined
    ## in ``<errno.h>``)
    errno*: int
  SQLiteError* = ref object of Error
    ## A SQLite database error. The ``code`` is defined in ``sqlite3.h``.
    code*: int
  FleeceError* = ref object of Error
    ## A Fleece error.
    code*: FleeceErrorCode
  NetworkError* = ref object of Error
    ## A Couchbase Lite network-related error; see the ``code`` for details.
    code*: NetworkErrorCode
  WebSocketError* = ref object of Error
    ## An HTTP or WebSocket error. The ``code`` is an HTTP status if less than
    ## 1000, else a WebSocket status.
    code*: int

proc message*(err: Error): string = $(message(err.cblErr))
  ## Gets the message associated with an error.

proc codeStr*(err: Error): string =
  ## Returns the name of the error's code.
  case err.cblErr.domain:
    of CBLDomain: $(cast[CBLErrorCode](err.cblErr.code))
    of FleeceDomain: $(cast[FleeceErrorCode](err.cblErr.code))
    of NetworkDomain: $(cast[NetworkErrorCode](err.cblErr.code))
    else: &"{err.cblErr.code}"

proc `$`*(err: Error): string =
  ## Formats an Error as a string, including the message, type and code.
  &"{err.message} ({err.cblErr.domain}.{err.cblErr.code}: {err.codeStr})"

#%%%% Internal error handling utilities:

# TODO: Avoid making these public

proc mkError*(err: CBLError): Error =
  case err.domain:
    of CBLDomain: CouchbaseLiteError(cblErr: err, code: ErrorCode(err.code))
    of POSIXDomain: POSIXError(cblErr: err, errno: err.code)
    of SQLiteDomain: SQLiteError(cblErr: err, code: err.code)
    of FleeceDomain: FleeceError(cblErr: err, code: FleeceErrorCode(err.code))
    of NetworkDomain: NetworkError(cblErr: err, code: NetworkErrorCode(err.code))
    of WebSocketDomain: WebSocketError(cblErr: err, code: err.code)
    else: CouchbaseLiteError(cblErr: err, code: ErrorCode(UnexpectedError))

proc throw*(err: CBLError) {.noreturn.} =
  raise mkError(err)

proc throw*(code: ErrorCode) =
  throw(CBLError(domain: cbl.CBLDomain, code: int32(code)))

proc checkBool*(fn: (ptr CBLError) -> bool) =
  var err: CBLError
  if not fn(addr err):
    throw(err)
