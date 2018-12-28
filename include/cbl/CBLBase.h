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

typedef CBL_ENUM(uint32_t, CBLErrorDomain) {
    CBLDomain = 1, // code is a Couchbase Lite Core error code (see below)
    CBLPOSIXDomain,        // code is an errno
    CBLSQLiteDomain,       // code is a SQLite error; see "sqlite3.h">"
    CBLFleeceDomain,       // code is a Fleece error; see "FleeceException.h"
    CBLNetworkDomain,      // code is a network error; see enum C4NetworkErrorCode, below
    CBLWebSocketDomain,    // code is a WebSocket close code (1000...1015) or HTTP error (300..599)

    CBLMaxErrorDomainPlus1
};

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


typedef struct {
    CBLErrorDomain domain;
    int32_t code;
    int32_t internal_info;
} CBLError;

char* cbl_error_message(const CBLError* _cblnonnull);
    // NOTE: Where a function returns "char*", that implies the string is heap-allocated
    // and it's the caller's responsibility to free it with free().

// Logging

typedef CBL_ENUM(uint8_t, CBLLogDomain) {
    // TODO: Add domains
};

typedef CBL_ENUM(uint8_t, CBLLogLevel) {
    CBLLogDebug,
    CBLLogVerbose,
    CBLLogInfo,
    CBLLogWarning,
    CBLLogError,
    CBLLogNone
};

void cbl_setLogLevel(CBLLogLevel, CBLLogDomain);


// Ref-Counting

typedef struct CBLRefCounted CBLRefCounted;
CBLRefCounted* cbl_retain(CBLRefCounted* _cblnonnull);
void cbl_release(CBLRefCounted*);

// Declares retain/release functions for TYPE
#define CBL_REFCOUNTED(TYPE, NAME) \
    static inline TYPE cbl_##NAME##_retain(TYPE _cblnonnull t)  {return (TYPE)cbl_retain((CBLRefCounted*)t);} \
    static inline void cbl_##NAME##_release(TYPE t) {cbl_release((CBLRefCounted*)t);}


// Object "classes"

typedef struct CBLDatabase   CBLDatabase;
typedef struct CBLDocument   CBLDocument;
typedef struct CBLQuery      CBLQuery;
typedef struct CBLResultSet  CBLResultSet;
typedef struct CBLReplicator CBLReplicator;


// Listeners

typedef struct CBLListenerToken CBLListenerToken;

void cbl_listener_remove(CBLListenerToken*);
    // NOTE: Instead of a separate function to remove each type of listener,
    // we can hide the listener type & owner in the token, and just use one function.


#ifdef __cplusplus
}
#endif
