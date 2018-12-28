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
#include "CBL_Compat.h"

#ifdef __cplusplus
extern "C" {
#endif


// Errors

/** Error domains, serving as namespaces for numeric error codes. */
typedef CBL_ENUM(uint32_t, CBLErrorDomain) {
    CBLDomain = 1, // code is a Couchbase Lite Core error code (see below)
    CBLPOSIXDomain,        // code is an errno
    CBLSQLiteDomain,       // code is a SQLite error; see "sqlite3.h"
    CBLFleeceDomain,       // code is a Fleece error; see "FleeceException.h"
    CBLNetworkDomain,      // code is a network error; see enum C4NetworkErrorCode, below
    CBLWebSocketDomain,    // code is a WebSocket close code (1000...1015) or HTTP error (300..599)

    CBLMaxErrorDomainPlus1
};

/** Couchbase Lite error codes, in the CBLDomain. */
typedef CBL_ENUM(int32_t,  CBLErrorCode) {
    CBLErrorAssertionFailed = 1,    // Internal assertion failure
    CBLErrorUnimplemented,          // Oops, an unimplemented API call
    CBLErrorUnsupportedEncryption,  // Unsupported encryption algorithm
    CBLErrorBadRevisionID,          // Invalid revision ID syntax
    CBLErrorCorruptRevisionData,    // Revision contains corrupted/unreadable data
    CBLErrorNotOpen,                // Database/KeyStore/index is not open
    CBLErrorNotFound,               // Document not found
    CBLErrorConflict,               // Document update conflict
    CBLErrorInvalidParameter,       // Invalid function parameter or struct value
    CBLErrorUnexpectedError, /*10*/ // Internal unexpected C++ exception
    CBLErrorCantOpenFile,           // Database file can't be opened; may not exist
    CBLErrorIOError,                // File I/O error
    CBLErrorMemoryError,            // Memory allocation failed (out of memory?)
    CBLErrorNotWriteable,           // File is not writeable
    CBLErrorCorruptData,            // Data is corrupted
    CBLErrorBusy,                   // Database is busy/locked
    CBLErrorNotInTransaction,       // Function must be called while in a transaction
    CBLErrorTransactionNotClosed,   // Database can't be closed while a transaction is open
    CBLErrorUnsupported,            // Operation not supported in this database
    CBLErrorNotADatabaseFile,/*20*/ // File is not a database, or encryption key is wrong
    CBLErrorWrongFormat,            // Database exists but not in the format/storage requested
    CBLErrorCrypto,                 // Encryption/decryption error
    CBLErrorInvalidQuery,           // Invalid query
    CBLErrorMissingIndex,           // No such index, or query requires a nonexistent index
    CBLErrorInvalidQueryParam,      // Unknown query param name, or param number out of range
    CBLErrorRemoteError,            // Unknown error from remote server
    CBLErrorDatabaseTooOld,         // Database file format is older than what I can open
    CBLErrorDatabaseTooNew,         // Database file format is newer than what I can open
    CBLErrorBadDocID,               // Invalid document ID
    CBLErrorCantUpgradeDatabase,/*30*/ // DB can't be upgraded (might be unsupported dev version)

    CBLNumErrorCodesPlus1
};

/** Network error codes, in the CBLNetworkDomain. */
typedef CBL_ENUM(int32_t,  CBLNetworkErrorCode) {
    CBLNetErrDNSFailure = 1,            // DNS lookup failed
    CBLNetErrUnknownHost,               // DNS server doesn't know the hostname
    CBLNetErrTimeout,
    CBLNetErrInvalidURL,
    CBLNetErrTooManyRedirects,
    CBLNetErrTLSHandshakeFailed,
    CBLNetErrTLSCertExpired,
    CBLNetErrTLSCertUntrusted,          // Cert isn't trusted for other reason
    CBLNetErrTLSClientCertRequired,
    CBLNetErrTLSClientCertRejected, // 10
    CBLNetErrTLSCertUnknownRoot,        // Self-signed cert, or unknown anchor cert
    CBLNetErrInvalidRedirect,           // Attempted redirect to invalid replication endpoint
};


/** A struct holding information about an error. It's declared on the stack by a caller, and
    its address is passed to an API function. If the function's return value indicates that
    there was an error (usually by returning NULL or false), then the CBLError will have been
    filled in with the details. */
typedef struct {
    CBLErrorDomain domain;      ///< Domain of errors; a namespace for the `code`.
    int32_t code;               ///< Error code, specific to the domain.
    int32_t internal_info;
} CBLError;

/** Returns a message describing an error.
    @note  It is the caller's responsibility to free the returned string by calling `free`. */
char* cbl_error_message(const CBLError* _cblnonnull);

// Logging

/** Subsystems that log information. */
typedef CBL_ENUM(uint8_t, CBLLogDomain) {
    // TODO: Add domains
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
void cbl_setLogLevel(CBLLogLevel, CBLLogDomain);


// Ref-Counting

typedef struct CBLRefCounted CBLRefCounted;

/** Increments an object's reference-count.
    Usually you'll call one of the type-safe synonyms specific to the object type,
    like `cbl_db_retain`. */
CBLRefCounted* cbl_retain(CBLRefCounted* _cblnonnull);

/** Decrements an object's reference-count, freeing the object if the count hits zero.
    Usually you'll call one of the type-safe synonyms specific to the object type,
    like `cbl_db_release`. */
void cbl_release(CBLRefCounted*);

// Declares retain/release functions for TYPE
#define CBL_REFCOUNTED(TYPE, NAME) \
    static inline TYPE cbl_##NAME##_retain(TYPE _cblnonnull t)  {return (TYPE)cbl_retain((CBLRefCounted*)t);} \
    static inline void cbl_##NAME##_release(TYPE t) {cbl_release((CBLRefCounted*)t);}


// Object "classes"

/** A connection to an open database. */
typedef struct CBLDatabase   CBLDatabase;
/** An in-memory copy of a document. */
typedef struct CBLDocument   CBLDocument;
/** A compiled database query. */
typedef struct CBLQuery      CBLQuery;
/** An iterator over the rows resulting from running a query. */
typedef struct CBLResultSet  CBLResultSet;
/** A background task that syncs a CBLDatabase with a remote server or peer. */
typedef struct CBLReplicator CBLReplicator;


// Listeners

/** An opaque 'cookie' representing a registered listener callback.
    It's returned from functions that register listeners, and used to remove a listener. */
typedef struct CBLListenerToken CBLListenerToken;

/** Removes a listener callback, given the token that was returned when it was added. */
void cbl_listener_remove(CBLListenerToken* _cblnonnull);


#ifdef __cplusplus
}
#endif
