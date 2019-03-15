//
// CBLBase.h
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
#ifdef CMAKE
#include "cbl_config.h"
#endif

#include "CBL_Compat.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup errors   Errors
     @{
    Types and constants for communicating errors from API calls. */

/** Error domains, serving as namespaces for numeric error codes. */
typedef CBL_ENUM(uint32_t, CBLErrorDomain) {
    CBLDomain = 1,         ///< code is a Couchbase Lite error code; see \ref CBLErrorCode
    CBLPOSIXDomain,        ///< code is a POSIX `errno`; see "errno.h"
    CBLSQLiteDomain,       ///< code is a SQLite error; see "sqlite3.h"
    CBLFleeceDomain,       ///< code is a Fleece error; see "FleeceException.h"
    CBLNetworkDomain,      ///< code is a network error; see \ref CBLNetworkErrorCode
    CBLWebSocketDomain,    ///< code is a WebSocket close code (1000...1015) or HTTP error (300..599)

    CBLMaxErrorDomainPlus1
};

/** Couchbase Lite error codes, in the CBLDomain. */
typedef CBL_ENUM(int32_t, CBLErrorCode) {
    CBLErrorAssertionFailed = 1,    ///< Internal assertion failure
    CBLErrorUnimplemented,          ///< Oops, an unimplemented API call
    CBLErrorUnsupportedEncryption,  ///< Unsupported encryption algorithm
    CBLErrorBadRevisionID,          ///< Invalid revision ID syntax
    CBLErrorCorruptRevisionData,    ///< Revision contains corrupted/unreadable data
    CBLErrorNotOpen,                ///< Database/KeyStore/index is not open
    CBLErrorNotFound,               ///< Document not found
    CBLErrorConflict,               ///< Document update conflict
    CBLErrorInvalidParameter,       ///< Invalid function parameter or struct value
    CBLErrorUnexpectedError, /*10*/ ///< Internal unexpected C++ exception
    CBLErrorCantOpenFile,           ///< Database file can't be opened; may not exist
    CBLErrorIOError,                ///< File I/O error
    CBLErrorMemoryError,            ///< Memory allocation failed (out of memory?)
    CBLErrorNotWriteable,           ///< File is not writeable
    CBLErrorCorruptData,            ///< Data is corrupted
    CBLErrorBusy,                   ///< Database is busy/locked
    CBLErrorNotInTransaction,       ///< Function must be called while in a transaction
    CBLErrorTransactionNotClosed,   ///< Database can't be closed while a transaction is open
    CBLErrorUnsupported,            ///< Operation not supported in this database
    CBLErrorNotADatabaseFile,/*20*/ ///< File is not a database, or encryption key is wrong
    CBLErrorWrongFormat,            ///< Database exists but not in the format/storage requested
    CBLErrorCrypto,                 ///< Encryption/decryption error
    CBLErrorInvalidQuery,           ///< Invalid query
    CBLErrorMissingIndex,           ///< No such index, or query requires a nonexistent index
    CBLErrorInvalidQueryParam,      ///< Unknown query param name, or param number out of range
    CBLErrorRemoteError,            ///< Unknown error from remote server
    CBLErrorDatabaseTooOld,         ///< Database file format is older than what I can open
    CBLErrorDatabaseTooNew,         ///< Database file format is newer than what I can open
    CBLErrorBadDocID,               ///< Invalid document ID
    CBLErrorCantUpgradeDatabase,/*30*/ ///< DB can't be upgraded (might be unsupported dev version)

    CBLNumErrorCodesPlus1
};

/** Network error codes, in the CBLNetworkDomain. */
typedef CBL_ENUM(int32_t,  CBLNetworkErrorCode) {
    CBLNetErrDNSFailure = 1,            ///< DNS lookup failed
    CBLNetErrUnknownHost,               ///< DNS server doesn't know the hostname
    CBLNetErrTimeout,                   ///< No response received before timeout
    CBLNetErrInvalidURL,                ///< Invalid URL
    CBLNetErrTooManyRedirects,          ///< HTTP redirect loop
    CBLNetErrTLSHandshakeFailed,        ///< Low-level error establishing TLS
    CBLNetErrTLSCertExpired,            ///< Server's TLS certificate has expired
    CBLNetErrTLSCertUntrusted,          ///< Cert isn't trusted for other reason
    CBLNetErrTLSClientCertRequired,     ///< Server requires client to have a TLS certificate
    CBLNetErrTLSClientCertRejected,     ///< Server rejected my TLS client certificate
    CBLNetErrTLSCertUnknownRoot,        ///< Self-signed cert, or unknown anchor cert
    CBLNetErrInvalidRedirect,           ///< Attempted redirect to invalid URL
};


/** A struct holding information about an error. It's declared on the stack by a caller, and
    its address is passed to an API function. If the function's return value indicates that
    there was an error (usually by returning NULL or false), then the CBLError will have been
    filled in with the details. */
typedef struct {
    CBLErrorDomain domain;      ///< Domain of errors; a namespace for the `code`.
    int32_t code;               ///< Error code, specific to the domain. 0 always means no error.
    int32_t internal_info;
} CBLError;

/** Returns a message describing an error.
    @note  It is the caller's responsibility to free the returned C string by calling `free`. */
char* CBLError_Message(const CBLError* _cbl_nonnull) CBLAPI;

/** @} */



/** \defgroup logging   Logging
     @{
    Managing messages that Couchbase Lite logs at runtime. */

/** Subsystems that log information. */
typedef CBL_ENUM(uint8_t, CBLLogDomain) {
    kCBLLogDomainAll,
    kCBLLogDomainDatabase,
    kCBLLogDomainQuery,
    kCBLLogDomainReplicator,
    kCBLLogDomainNetwork,
};

/** Levels of log messages. Higher values are more important/severe. */
typedef CBL_ENUM(uint8_t, CBLLogLevel) {
    CBLLogDebug,
    CBLLogVerbose,
    CBLLogInfo,
    CBLLogWarning,
    CBLLogError,
    CBLLogNone
};

/** Sets the detail level of logging. */
void CBL_SetLogLevel(CBLLogLevel, CBLLogDomain) CBLAPI;

void CBL_Log(CBLLogDomain, CBLLogLevel, const char *format _cbl_nonnull, ...);

/** @} */



/** \defgroup refcounting   Reference Counting
     @{
    Couchbase Lite "objects" are reference-counted; these functions are the shared
    `retain` and `release` operations. (But there are type-safe equivalents defined for each
    class, so you can call \ref cbl_db_release() on a database, for instance, without having to
    type-cast.)
 */

typedef struct CBLRefCounted CBLRefCounted;

/** Increments an object's reference-count.
    Usually you'll call one of the type-safe synonyms specific to the object type,
    like \ref cbl_db_retain` */
CBLRefCounted* CBL_Retain(CBLRefCounted*) CBLAPI;

/** Decrements an object's reference-count, freeing the object if the count hits zero.
    Usually you'll call one of the type-safe synonyms specific to the object type,
    like \ref cbl_db_release. */
void CBL_Release(CBLRefCounted*) CBLAPI;

/** Returns the total number of Couchbase Lite objects. Useful for leak checking. */
unsigned CBL_InstanceCount(void) CBLAPI;

/** Logs the class and address of each Couchbase Lite object. Useful for leak checking.
    @note  May only be functional in debug builds of Couchbase Lite. */
void CBL_DumpInstances(void) CBLAPI;

// Declares retain/release functions for TYPE
#define CBL_REFCOUNTED(TYPE, NAME) \
    static inline const TYPE CBL##NAME##_Retain(const TYPE _cbl_nonnull t) \
                                            {return (const TYPE)CBL_Retain((CBLRefCounted*)t);} \
    static inline void CBL##NAME##_Release(const TYPE t) {CBL_Release((CBLRefCounted*)t);}

/** @} */



/** \defgroup database  Database
     @{ */
/** A connection to an open database. */
typedef struct CBLDatabase   CBLDatabase;
/** @} */

/** \defgroup documents  Documents
     @{ */
/** An in-memory copy of a document. */
typedef struct CBLDocument   CBLDocument;
/** @} */

/** \defgroup blobs Blobs
     @{ */
/** A binary data value associated with a document. */
typedef struct CBLBlob      CBLBlob;
/** @} */

/** \defgroup queries  Queries
     @{ */
/** A compiled database query. */
typedef struct CBLQuery      CBLQuery;
/** An iterator over the rows resulting from running a query. */
typedef struct CBLResultSet  CBLResultSet;
/** @} */

/** \defgroup replication  Replication
     @{ */
/** A background task that syncs a CBLDatabase with a remote server or peer. */
typedef struct CBLReplicator CBLReplicator;
/** @} */



/** \defgroup listeners   Listeners
     @{
    Every API function that registers a listener callback returns an opaque token representing
    the registered callback. To unregister any type of listener, call \ref CBLListener_Remove.
 */

/** An opaque 'cookie' representing a registered listener callback.
    It's returned from functions that register listeners, and used to remove a listener. */
typedef struct CBLListenerToken CBLListenerToken;

/** Removes a listener callback, given the token that was returned when it was added. */
void CBLListener_Remove(CBLListenerToken*) CBLAPI;


/** @} */

#ifdef __cplusplus
}
#endif
