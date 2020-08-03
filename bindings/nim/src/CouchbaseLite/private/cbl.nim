## *** NOTE: DO NOT MACHINE-UPDATE THIS FILE ***
##
## This started out as a machine-generated file (produced by `gen-bindings.sh`),
## but it has been hand-edited to fix issues that kept it from compiling.
##
## If you run the script again, it will generate a _new_ file `CouchbaseLite-new.nim`.
## You'll need to merge new/changed declarations from that file into this one, by hand.
##
## Status: Up-to-date as of 5 May 2020, commit 036cd9f7 "Export kCBLAuthDefaultCookieName...",
##         except I've left out the new "_s"-suffixed alternate functions, which Nim doesn't need.


when defined(Linux):
  {.push dynlib: "libCouchbaseLiteC.so".}
elif defined(MacOS) or defined(MacOSX):
  {.push dynlib: "libCouchbaseLiteC.dylib".}
elif defined(Windows):
  {.push dynlib: "libCouchbaseLiteC.dll".}


##
##  CBLBase.h
##
##  Copyright (c) 2018 Couchbase, Inc All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
import fl

## \defgroup errors   Errors
##     Types and constants for communicating errors from API calls.
## CBLError domains, serving as namespaces for numeric error codes.
type
  CBLErrorDomain* {.size: sizeof(cint).} = enum
    CBLDomain = 1, ## code is a Couchbase Lite error code; see \ref CBLErrorCode
    POSIXDomain,   ## code is a POSIX `errno`; see "errno.h"
    SQLiteDomain,  ## code is a SQLite error; see "sqlite3.h"
    FleeceDomain,  ## code is a Fleece error; see "FleeceException.h"
    NetworkDomain, ## code is a network error; see \ref CBLNetworkErrorCode
    WebSocketDomain, ## code is a WebSocket close code (1000...1015) or HTTP error (300..599)
    MaxErrorDomainPlus1


## Couchbase Lite error codes, in the CBLDomain.
type
  CBLErrorCode* {.size: sizeof(cint).} = enum
    ErrorAssertionFailed = 1,   ## Internal assertion failure
    ErrorUnimplemented,         ## Oops, an unimplemented API call
    ErrorUnsupportedEncryption, ## Unsupported encryption algorithm
    ErrorBadRevisionID,         ## Invalid revision ID syntax
    ErrorCorruptRevisionData,   ## Revision contains corrupted/unreadable data
    ErrorNotOpen,               ## CBLDatabase/KeyStore/index is not open
    ErrorNotFound,              ## CBLDocument not found
    ErrorConflict,              ## CBLDocument update conflict
    ErrorInvalidParameter,      ## Invalid function parameter or struct value
    ErrorUnexpectedError,       ## Internal unexpected C++ exception
    ErrorCantOpenFile,          ## CBLDatabase file can't be opened; may not exist
    ErrorIOError,               ## File I/O error
    ErrorMemoryError,           ## Memory allocation failed (out of memory?)
    ErrorNotWriteable,          ## File is not writeable
    ErrorCorruptData,           ## Data is corrupted
    ErrorBusy,                  ## CBLDatabase is busy/locked
    ErrorNotInTransaction,      ## Function must be called while in a transaction
    ErrorTransactionNotClosed,  ## CBLDatabase can't be closed while a transaction is open
    ErrorUnsupported,           ## Operation not supported in this database
    ErrorNotADatabaseFile,      ## File is not a database, or encryption key is wrong
    ErrorWrongFormat,           ## CBLDatabase exists but not in the format/storage requested
    ErrorCrypto,                ## Encryption/decryption error
    ErrorInvalidQuery,          ## Invalid query
    ErrorMissingIndex,          ## No such index, or query requires a nonexistent index
    ErrorInvalidQueryParam,     ## Unknown query param name, or param number out of range
    ErrorRemoteError,           ## Unknown error from remote server
    ErrorDatabaseTooOld,        ## CBLDatabase file format is older than what I can open
    ErrorDatabaseTooNew,        ## CBLDatabase file format is newer than what I can open
    ErrorBadDocID,              ## Invalid document ID
    ErrorCantUpgradeDatabase,   ## DB can't be upgraded (might be unsupported dev version)
    NumErrorCodesPlus1


## Network error codes, in the CBLNetworkDomain.
type
  NetworkErrorCode* {.size: sizeof(cint).} = enum
    NetErrDNSFailure = 1,        ## DNS lookup failed
    NetErrUnknownHost,           ## DNS server doesn't know the hostname
    NetErrTimeout,               ## No response received before timeout
    NetErrInvalidURL,            ## Invalid URL
    NetErrTooManyRedirects,      ## HTTP redirect loop
    NetErrTLSHandshakeFailed,    ## Low-level error establishing TLS
    NetErrTLSCertExpired,        ## Server's TLS certificate has expired
    NetErrTLSCertUntrusted,      ## Cert isn't trusted for other reason
    NetErrTLSClientCertRequired, ## Server requires client to have a TLS certificate
    NetErrTLSClientCertRejected, ## Server rejected my TLS client certificate
    NetErrTLSCertUnknownRoot,    ## Self-signed cert, or unknown anchor cert
    NetErrInvalidRedirect,       ## Attempted redirect to invalid URL
    NetErrUnknown,               ## Unknown networking error
    NetErrTLSCertRevoked,        ## Server's cert has been revoked
    NetErrTLSCertNameMismatch    ## Server cert's name does not match DNS name


## A struct holding information about an error. It's declared on the stack by a caller, and
##     its address is passed to an API function. If the function's return value indicates that
##     there was an error (usually by returning NULL or false), then the CBLError will have been
##     filled in with the details.
type
  CBLError* {.byref.} = object
    domain*: CBLErrorDomain ## Domain of errors; a namespace for the `code`.
    code*: int32            ## CBLError code, specific to the domain. 0 always means no error.
    internalInfo: int32


## Returns a message describing an error.
proc message*(err: var CBLError): cstring {.importc: "CBLError_Message".}
proc message_s*(err: var CBLError): FLSliceResult {.
    importc: "CBLError_Message_s".}


## \defgroup other_types   Other Types
## A date/time representation used for document expiration (and in date/time queries.)
##     Measured in milliseconds since the Unix epoch (1/1/1970, midnight UTC.)
type
  CBLTimestamp* = int64

## Returns the current time, in milliseconds since 1/1/1970.
proc now*(): CBLTimestamp {.importc: "CBL_Now".}

## \defgroup refcounting   Reference Counting
##     Couchbase Lite "objects" are reference-counted; the functions below are the shared
##     _retain_ and _release_ operations. (But there are type-safe equivalents defined for each
##     class, so you can call \ref CBLDatabase_Release() on a database, for instance, without having to
##     type-cast.)
##
##     API functions that **create** a ref-counted object (typically named `..._New()` or `..._Create()`)
##     return the object with a ref-count of 1; you are responsible for releasing the reference
##     when you're done with it, or the object will be leaked.
##
##     Other functions that return an **existing** ref-counted object do not modify its ref-count.
##     You do _not_ need to release such a reference. But if you're keeping a reference to the object
##     for a while, you should retain the reference to ensure it stays alive, and then release it when
##     finished (to balance the retain.)
##
type
  CBLRefCounted = ptr object

proc retain(a1: CBLRefCounted): CBLRefCounted {.importc: "CBL_Retain", discardable.}
proc release(a1: CBLRefCounted) {.importc: "CBL_Release".}

## Returns the total number of Couchbase Lite objects. Useful for leak checking.
proc instanceCount*(): cuint {.importc: "CBL_InstanceCount".}

## Logs the class and address of each Couchbase Lite object. Useful for leak checking.
proc dumpInstances*() {.importc: "CBL_DumpInstances".}

## \defgroup database  CBLDatabase
## A connection to an open database.
type
  CBLDatabase* = ptr object

proc retain*(obj: CBLDatabase): CBLDatabase {.inline, discardable.} =
  retain(cast[CBLRefCounted](obj))
  return obj
proc release*(obj: CBLDatabase) {.inline.} =
  release(cast[CBLRefCounted](obj))


## \defgroup documents  Documents
## An in-memory copy of a document.
##     CBLDocument objects can be mutable or immutable. Immutable objects are referenced by _const_
##     pointers; mutable ones by _non-const_ pointers. This prevents you from accidentally calling
##     a mutable-document function on an immutable document.
type
  CBLDocument* = ptr object

proc retain*(obj: CBLDocument): CBLDocument {.inline, discardable.} =
  retain(cast[CBLRefCounted](obj))
  return obj
proc release*(obj: CBLDocument) {.inline.} =
  release(cast[CBLRefCounted](obj))

## \defgroup blobs Blobs
## A binary data value associated with a \ref CBLDocument.
type
  CBLBlob* = ptr object

proc retain*(obj: CBLBlob): CBLBlob {.inline, discardable.} =
  retain(cast[CBLRefCounted](obj))
  return obj
proc release*(obj: CBLBlob) {.inline.} =
  release(cast[CBLRefCounted](obj))

## \defgroup queries  Queries
## A compiled database query.
type
  CBLQuery* = ptr object

proc retain*(obj: CBLQuery): CBLQuery {.inline, discardable.} =
  retain(cast[CBLRefCounted](obj))
  return obj
proc release*(obj: CBLQuery) {.inline.} =
  release(cast[CBLRefCounted](obj))

## An iterator over the rows resulting from running a query.
type
  CBLResultSet* = ptr object

proc retain*(obj: CBLResultSet): CBLResultSet {.inline, discardable.} =
  retain(cast[CBLRefCounted](obj))
  return obj
proc release*(obj: CBLResultSet) {.inline.} =
  release(cast[CBLRefCounted](obj))

## \defgroup replication  Replication
## A background task that syncs a \ref CBLDatabase with a remote server or peer.
type
  CBLReplicator* = ptr object

proc retain*(obj: CBLReplicator): CBLReplicator {.inline, discardable.} =
  retain(cast[CBLRefCounted](obj))
  return obj
proc release*(obj: CBLReplicator) {.inline.} =
  release(cast[CBLRefCounted](obj))

## \defgroup listeners   Listeners
##     Every API function that registers a listener callback returns an opaque token representing
##     the registered callback. To unregister any type of listener, call \ref CBLListener_Remove.
##
##     The steps to creating a listener are:
##     1. Define the type of contextual information the callback needs. This is usually one of
##         your objects, or a custom struct.
##     2. Implement the listener function:
##       - The parameters and return value must match the callback defined in the API.
##       - The first parameter is always a `void*` that points to your contextual
##           information, so cast that to the actual pointer type.
##       - **The function may be called on a background thread!** And since the CBL API is not itself
##           thread-safe, you'll need to take special precautions if you want to call the API
##           from your listener, such as protecting all of your calls (inside and outside the
##           listener) with a mutex. It's safer to use \ref CBLDatabase_BufferNotifications to
##           schedule listener callbacks to a time of your own choosing, such as your thread's
##           event loop; see that function's docs for details.
##     3. To register the listener, call the relevant `AddListener` function.
##       - The parameters will include the CBL object to observe, the address of your listener
##         function, and a pointer to the contextual information. (That pointer needs to remain
##         valid for as long as the listener is registered, so it can't be a pointer to a local
##         variable.)
##       - The return value is a \ref CBLListenerToken pointer; save that.
##     4. To unregister the listener, pass the \ref CBLListenerToken to \ref CBLListener_Remove.
##       - You **must** unregister the listener before the contextual information pointer is
##         invalidated, e.g. before freeing the object it points to.
##
## An opaque 'cookie' representing a registered listener callback.
##     It's returned from functions that register listeners, and used to remove a listener by
##     calling \ref CBLListener_Remove.
type
  CBLListenerToken* = ptr object

## Removes a listener callback, given the token that was returned when it was added.
proc remove*(a1: CBLListenerToken) {.importc: "CBLListener_Remove".}




##
##  CBLLog.h
##
##  Copyright © 2019 Couchbase. All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##

## * \defgroup logging   Logging
##     Managing messages that Couchbase Lite logs at runtime.
## * Subsystems that log information.



type
  CBLLogDomain* {.size: sizeof(cint).} = enum
    kLogDomainAll, kLogDomainDatabase, kLogDomainQuery, kLogDomainReplicator, kLogDomainNetwork

## * Levels of log messages. Higher values are more important/severe. Each level includes the lower ones.
type
  CBLLogLevel* {.size: sizeof(cint).} = enum
    CBLLogDebug,   ## /< Extremely detailed messages, only written by debug builds of CBL.
    CBLLogVerbose, ## /< Detailed messages about normally-unimportant stuff.
    CBLLogInfo,    ## /< Messages about ordinary behavior.
    CBLLogWarning, ## /< Messages warning about unlikely and possibly bad stuff.
    CBLLogError,   ## /< Messages about errors
    CBLLogNone     ## /< Disables logging entirely.


## * Formats and writes a message to the log, in the given domain at the given level.
##     \warning This function takes a `printf`-style format string, with extra parameters to match the format placeholders, and has the same security vulnerabilities as other `printf`-style functions.
##     If you are logging a fixed string, call \ref CBL_Log_s instead, otherwise any `%` characters in the
##     `format` string will be misinterpreted as placeholders and the dreaded Undefined Behavior will result,
##     possibly including crashes or overwriting the stack.
##     @param domain  The log domain to associate this message with.
##     @param level  The severity of the message. If this is lower than the current minimum level for the domain
##                  (as set by \ref CBL_SetLogLevel), nothing is logged.
##     @param format  A `printf`-style format string. `%` characters in this string introduce parameters,
##                  and corresponding arguments must follow.
proc log*(domain: CBLLogDomain; level: CBLLogLevel; format: cstring) {.varargs,
    importc: "CBL_Log".}

## * Writes a pre-formatted message to the log, exactly as given.
##     @param domain  The log domain to associate this message with.
##     @param level  The severity of the message. If this is lower than the current minimum level for the domain
##                  (as set by \ref CBL_SetLogLevel), nothing is logged.
##     @param message  The exact message to write to the log.

proc logString*(domain: CBLLogDomain; level: CBLLogLevel; message: Slice) {.
    importc: "CBL_Log_s".}
## * \name Console Logging and Custom Logging
## * A logging callback that the application can register.
##     @param domain  The domain of the message; \ref kCBLLogDomainAll if it doesn't fall into a specific domain.
##     @param level  The severity level of the message.
##     @param message  The actual formatted message.

type
  CBLLogCallback* = proc (domain: CBLLogDomain; level: CBLLogLevel;
      message: cstring)

## * Gets the current log level for debug console logging.
##     Only messages at this level or higher will be logged to the console or callback.
proc consoleLevel*(): CBLLogLevel {.importc: "CBLLog_ConsoleLevel".}

## * Sets the detail level of logging.
##     Only messages whose level is ≥ the given level will be logged to the console or callback.
proc setConsoleLevel*(a1: CBLLogLevel) {.importc: "CBLLog_SetConsoleLevel".}

## * Returns true if a message with the given domain and level would be logged to the console.
proc willLogToConsole*(domain: CBLLogDomain; level: CBLLogLevel): bool {.
    importc: "CBLLog_WillLogToConsole".}

## * Gets the current log callback.
proc callback*(): CBLLogCallback {.importc: "CBLLog_Callback".}

## * Sets the callback for receiving log messages. If set to NULL, no messages are logged to the console.
proc setCallback*(a1: CBLLogCallback) {.importc: "CBLLog_SetCallback".}

## * \name Log File Configuration
## * The properties for configuring logging to files.
##     @warning `usePlaintext` results in significantly larger log files and higher CPU usage that may slow
##             down your app; we recommend turning it off in production.
type
  CBLLogFileConfiguration* {.bycopy.} = object
    directory*: cstring ## /< The directory where log files will be created.
    maxRotateCount*: uint32 ## /< Max number of older logs to keep (i.e. total number will be one more.)
    maxSize*: csize_t ## /< The size in bytes at which a file will be rotated out (best effort).
    usePlaintext*: bool ## /< Whether or not to log in plaintext (as opposed to binary)


## * Gets the current file logging configuration.
proc fileConfig*(): ptr CBLLogFileConfiguration {.importc: "CBLLog_FileConfig".}

## * Sets the file logging configuration.
proc setFileConfig*(a1: CBLLogFileConfiguration) {.
    importc: "CBLLog_SetFileConfig".}




##
##  CBLDatabase.h
##
##  Copyright (c) 2018 Couchbase, Inc All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
## \defgroup database   CBLDatabase
##     A \ref CBLDatabase is both a filesystem object and a container for documents.

## \name  CBLDatabase configuration
## Flags for how to open a database.
type
  CBLDatabaseFlags* {.size: sizeof(cint).} = enum
    kDatabaseCreate = 1,   ## Create the file if it doesn't exist
    kDatabaseReadOnly = 2, ## Open file read-only
    kDatabaseNoUpgrade = 4 ## Disable upgrading an older-version database


## Encryption algorithms (available only in the Enterprise Edition).
type
  CBLEncryptionAlgorithm* {.size: sizeof(cint).} = enum
    kEncryptionNone = 0, ## No encryption (default)
                         ## #ifdef COUCHBASE_ENTERPRISE
    kEncryptionAES256    ## AES with 256-bit key
                         ## #endif


## Encryption key sizes (in bytes).
type
  CBLEncryptionKeySize* {.size: sizeof(cint).} = enum
    kEncryptionKeySizeAES256 = 32 ## Key size for \ref kCBLEncryptionAES256


## Encryption key specified in a \ref CBLDatabaseConfiguration.
type
  CBLEncryptionKey* {.bycopy.} = object
    algorithm*: CBLEncryptionAlgorithm ## Encryption algorithm
    bytes*: array[32, uint8]           ## Raw key data


## CBLDatabase configuration options.
type
  CBLDatabaseConfiguration* = object
    directory*: cstring      ## The parent directory of the database
    flags*: CBLDatabaseFlags ## Options for opening the database
    encryptionKey*: ptr CBLEncryptionKey ## The database's encryption key (if any)


## \name  CBLDatabase file operations
##     These functions operate on database files without opening them.
##
## Returns true if a database with the given name exists in the given directory.
##                         absolute or relative path to the database.
proc databaseExists*(name: cstring; inDirectory: cstring): bool {.
    importc: "CBL_DatabaseExists".}

## Copies a database file to a new location, and assigns it a new internal UUID to distinguish
##     it from the original database when replicating.
proc copyDatabase*(fromPath: cstring; toName: cstring;
    config: CBLDatabaseConfiguration; err: var CBLError): bool {.
    importc: "CBL_CopyDatabase".}

## Deletes a database file. If the database file is open, an error is returned.
##                         absolute or relative path to the database.
##                 (You can tell the last two cases apart by looking at \p outError.)
proc deleteDatabase*(name: cstring; inDirectory: cstring;
    err: var CBLError): bool {.importc: "CBL_DeleteDatabase".}

## \name  CBLDatabase lifecycle
##     Opening, closing, and managing open databases.
##
## Opens a database, or creates it if it doesn't exist yet, returning a new \ref CBLDatabase
##     instance.
##     It's OK to open the same database file multiple times. Each \ref CBLDatabase instance is
##     independent of the others (and must be separately closed and released.)
proc openDatabase*(name: cstring; config: ptr CBLDatabaseConfiguration;
    err: var CBLError): CBLDatabase {.importc: "CBLDatabase_Open".}

## Closes an open database.
proc close*(a1: CBLDatabase; err: var CBLError): bool {.
    importc: "CBLDatabase_Close".}

## Closes and deletes a database. If there are any other connections to the database,
##     an error is returned.
proc delete*(a1: CBLDatabase; err: var CBLError): bool {.
    importc: "CBLDatabase_Delete".}

## Compacts a database file.
proc compact*(a1: CBLDatabase; err: var CBLError): bool {.
    importc: "CBLDatabase_Compact".}

## Begins a batch operation, similar to a transaction. You **must** later call \ref
##     CBLDatabase_EndBatch to end (commit) the batch.
##             the batch operation ends.
proc beginBatch*(a1: CBLDatabase; err: var CBLError): bool {.
    importc: "CBLDatabase_BeginBatch".}

## Ends a batch operation. This **must** be called after \ref CBLDatabase_BeginBatch.
proc endBatch*(a1: CBLDatabase; err: var CBLError): bool {.
    importc: "CBLDatabase_EndBatch".}

## Returns the nearest future time at which a document in this database will expire,
##     or 0 if no documents will expire.
proc nextDocExpiration*(a1: CBLDatabase): CBLTimestamp {.
    importc: "CBLDatabase_NextDocExpiration".}

## Purges all documents whose expiration time has passed.
proc purgeExpiredDocuments*(db: CBLDatabase; err: var CBLError): int64 {.
    importc: "CBLDatabase_PurgeExpiredDocuments".}

## \name  CBLDatabase accessors
##     Getting information about a database.
##
## Returns the database's name.
proc name*(a1: CBLDatabase): cstring {.importc: "CBLDatabase_Name".}

## Returns the database's full filesystem path.
proc path*(a1: CBLDatabase): cstring {.importc: "CBLDatabase_Path".}

## Returns the number of documents in the database.
proc count*(a1: CBLDatabase): uint64 {.importc: "CBLDatabase_Count".}

## Returns the database's configuration, as given when it was opened.
proc config*(a1: CBLDatabase): CBLDatabaseConfiguration {.
    importc: "CBLDatabase_Config".}

## \name  CBLDatabase listeners
##     A database change listener lets you detect changes made to all documents in a database.
##     (If you only want to observe specific documents, use a \ref CBLDocumentChangeListener instead.)
##     listeners will be notified of changes made by other database instances.
## A database change listener callback, invoked after one or more documents are changed on disk.
##                     prepared for that, you may want to use \ref CBLDatabase_BufferNotifications
##                     so that listeners will be called in a safe context.
type
  DatabaseChangeListener* = proc (context: pointer; db: CBLDatabase;
      numDocs: cuint; docIDs: cstringArray)

## Registers a database change listener callback. It will be called after one or more
##     documents are changed on disk.
##             listener.
proc addChangeListener*(db: CBLDatabase; listener: DatabaseChangeListener;
    context: pointer): CBLListenerToken {.
    importc: "CBLDatabase_AddChangeListener".}

##  end of outer \defgroup
## \defgroup listeners   Listeners
## \name  Scheduling notifications
##     Applications may want control over when Couchbase Lite notifications (listener callbacks)
##     happen. They may want them called on a specific thread, or at certain times during an event
##     loop. This behavior may vary by database, if for instance each database is associated with a
##     separate thread.
##
##     The API calls here enable this. When notifications are "buffered" for a database, calls to
##     listeners will be deferred until the application explicitly allows them. Instead, a single
##     callback will be issued when the first notification becomes available; this gives the app a
##     chance to schedule a time when the notifications should be sent and callbacks called.
##
## Callback indicating that the database (or an object belonging to it) is ready to call one
##     or more listeners. You should call \ref CBLDatabase_SendNotifications at your earliest
##     convenience, in the context (thread, dispatch queue, etc.) you want them to run.
##             is called. If you don't respond by (sooner or later) calling that function,
##             you will not be informed that any listeners are ready.
##               possible, just scheduling a future call to \ref CBLDatabase_SendNotifications.
type
  NotificationsReadyCallback* = proc (context: pointer; db: CBLDatabase)

## Switches the database to buffered-notification mode. Notifications for objects belonging
##     to this database (documents, queries, replicators, and of course the database) will not be
##     called immediately; your \ref CBLNotificationsReadyCallback will be called instead.
proc bufferNotifications*(db: CBLDatabase; callback: NotificationsReadyCallback;
    context: pointer) {.importc: "CBLDatabase_BufferNotifications".}

## Immediately issues all pending notifications for this database, by calling their listener
##     callbacks.
proc sendNotifications*(db: CBLDatabase) {.
    importc: "CBLDatabase_SendNotifications".}




##  CBLDocument.h
##
##  Copyright (c) 2018 Couchbase, Inc All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
## \defgroup documents   Documents
##     A \ref CBLDocument is essentially a JSON object with an ID string that's unique in its database.
##
## \name  CBLDocument lifecycle
## Conflict-handling options when saving or deleting a document.
type
  CBLConcurrencyControl* {.size: sizeof(
      cint).} = enum ## The current save/delete will overwrite a conflicting revision if there is a conflict.
    kConcurrencyControlLastWriteWins, ## The current save/delete will fail if there is a conflict.
    kConcurrencyControlFailOnConflict


## Custom conflict handler for use when saving or deleting a document. This handler is called
##     if the save would cause a conflict, i.e. if the document in the database has been updated
##     (probably by a pull replicator, or by application code on another thread)
##     since it was loaded into the CBLDocument being saved.
type
  CBLSaveConflictHandler* = proc (context: pointer;
      documentBeingSaved: CBLDocument; conflictingDocument: CBLDocument): bool

## Reads a document from the database, creating a new (immutable) \ref CBLDocument object.
##     Each call to this function creates a new object (which must later be released.)
##             \ref CBLDatabase_GetMutableDocument instead.
proc getDocument*(database: CBLDatabase; docID: cstring): CBLDocument {.
    importc: "CBLDatabase_GetDocument".}

## Saves a (mutable) document to the database.
##     If a conflicting revision has been saved since \p doc was loaded, the \p concurrency
##     parameter specifies whether the save should fail, or the conflicting revision should
##     be overwritten with the revision being saved.
##     If you need finer-grained control, call \ref CBLDatabase_SaveDocumentResolving instead.
proc saveDocument*(db: CBLDatabase; doc: CBLDocument;
    concurrency: CBLConcurrencyControl; err: var CBLError): CBLDocument {.
    importc: "CBLDatabase_SaveDocument".}

## Saves a (mutable) document to the database. This function is the same as \ref
##     CBLDatabase_SaveDocument, except that it allows for custom conflict handling in the event
##     that the document has been updated since \p doc was loaded.
proc saveDocumentResolving*(db: CBLDatabase; doc: CBLDocument;
    conflictHandler: CBLSaveConflictHandler; context: pointer;
    err: var CBLError): CBLDocument {.
    importc: "CBLDatabase_SaveDocumentResolving".}

## Deletes a document from the database. Deletions are replicated.
proc delete*(document: CBLDocument; concurrency: CBLConcurrencyControl;
    err: var CBLError): bool {.importc: "CBLDocument_Delete".}

## Purges a document. This removes all traces of the document from the database.
##     Purges are _not_ replicated. If the document is changed on a server, it will be re-created
##     when pulled.
##           simpler shortcut.
proc purge*(document: CBLDocument; err: var CBLError): bool {.
    importc: "CBLDocument_Purge".}

## Purges a document, given only its ID.
##             code will be zero.
##
proc purgeDocumentByID*(database: CBLDatabase; docID: cstring;
    err: var CBLError): bool {.importc: "CBLDatabase_PurgeDocumentByID".}

## \name  Mutable documents
##     The type `CBLDocument*` without a `const` qualifier refers to a _mutable_ document instance.
##     A mutable document exposes its properties as a mutable dictionary, so you can change them
##     in place and then call \ref CBLDatabase_SaveDocument to persist the changes.
##
## Reads a document from the database, in mutable form that can be updated and saved.
##     (This function is otherwise identical to \ref CBLDatabase_GetDocument.)
proc getMutableDocument*(database: CBLDatabase; docID: cstring): CBLDocument {.
    importc: "CBLDatabase_GetMutableDocument".}

## Creates a new, empty document in memory. It will not be added to a database until saved.
proc newDocument*(docID: cstring): CBLDocument {.importc: "CBLDocument_New".}

## Creates a new mutable CBLDocument instance that refers to the same document as the original.
##     If the original document has unsaved changes, the new one will also start out with the same
##     changes; but mutating one document thereafter will not affect the other.
proc mutableCopy*(original: CBLDocument): CBLDocument {.
    importc: "CBLDocument_MutableCopy".}

## \name  CBLDocument properties and metadata
##     A document's body is essentially a JSON object. The properties are accessed in memory
##     using the Fleece API, with the body itself being a \ref FLDict "dictionary").
##
## Returns a document's ID.
proc id*(doc: CBLDocument): cstring {.importc: "CBLDocument_ID".}

## Returns a document's revision ID, which is a short opaque string that's guaranteed to be
##     unique to every change made to the document.
##     If the document doesn't exist yet, this function returns NULL.
proc revisionID*(doc: CBLDocument): cstring {.
    importc: "CBLDocument_RevisionID".}

## Returns a document's current sequence in the local database.
##     This number increases every time the document is saved, and a more recently saved document
##     will have a greater sequence number than one saved earlier, so sequences may be used as an
##     abstract 'clock' to tell relative modification times.
proc sequence*(doc: CBLDocument): uint64 {.importc: "CBLDocument_Sequence".}

## Returns a document's properties as a dictionary.
##             If you need to use any properties after releasing the document, you must retain them
##             by calling \ref FLValue_Retain (and of course later release them.)
##            underlying dictionary itself is mutable and could be modified through a mutable
##            reference obtained via \ref CBLDocument_MutableProperties. If you need to preserve the
##            properties, call \ref FLDict_MutableCopy to make a deep copy.
proc properties*(doc: CBLDocument): FLDict {.importc: "CBLDocument_Properties".}

## Returns a mutable document's properties as a mutable dictionary.
##     You may modify this dictionary and then call \ref CBLDatabase_SaveDocument to persist the changes.
##            same collection returned by \ref CBLDocument_Properties.
##             If you need to use any properties after releasing the document, you must retain them
##             by calling \ref FLValue_Retain (and of course later release them.)
proc mutableProperties*(doc: CBLDocument): FLMutableDict {.
    importc: "CBLDocument_MutableProperties".}

## Sets a mutable document's properties.
##     Call \ref CBLDatabase_SaveDocument to persist the changes.
##            releasing any retained reference(s) you have to it.
proc setProperties*(doc: CBLDocument; properties: FLMutableDict) {.
    importc: "CBLDocument_SetProperties".}

proc createDocumentFleeceDoc*(doc: CBLDocument): FLDoc {.
    importc: "CBLDocument_CreateFleeceDoc".}

## Returns a document's properties as a null-terminated JSON string.
proc propertiesAsJSON*(doc: CBLDocument): cstring {.
    importc: "CBLDocument_PropertiesAsJSON".}

## Sets a mutable document's properties from a JSON string.
proc setPropertiesAsJSON*(doc: CBLDocument; json: cstring;
    err: var CBLError): bool {.importc: "CBLDocument_SetPropertiesAsJSON".}

## Returns the time, if any, at which a given document will expire and be purged.
##     Documents don't normally expire; you have to call \ref CBLDatabase_SetDocumentExpiration
##     to set a document's expiration time.
##              or 0 if the document does not have an expiration,
##              or -1 if the call failed.
proc getDocumentExpiration*(db: CBLDatabase; docID: cstring;
    err: var CBLError): CBLTimestamp {.
    importc: "CBLDatabase_GetDocumentExpiration".}

## Sets or clears the expiration time of a document.
##             \ref CBLDatabase_PurgeExpiredDocuments when the time comes, to make it happen.
##                         or 0 if the document should never expire.
proc setDocumentExpiration*(db: CBLDatabase; docID: cstring;
    expiration: CBLTimestamp; err: var CBLError): bool {.
    importc: "CBLDatabase_SetDocumentExpiration".}

## \name  CBLDocument listeners
##     A document change listener lets you detect changes made to a specific document after they
##     are persisted to the database.
##     document listeners will be notified of changes made by other database instances.
##
## A document change listener callback, invoked after a specific document is changed on disk.
##                     prepared for that, you may want to use \ref CBLDatabase_BufferNotifications
##                     so that listeners will be called in a safe context.
type
  DocumentChangeListener* = proc (context: pointer; db: CBLDatabase;
      docID: cstring)

## Registers a document change listener callback. It will be called after a specific document
##     is changed on disk.
##             listener.
proc addDocumentChangeListener*(db: CBLDatabase; docID: cstring;
    listener: DocumentChangeListener; context: pointer): CBLListenerToken {.
    importc: "CBLDatabase_AddDocumentChangeListener".}




##
##  CBLBlob.h
##
##  Copyright (c) 2018 Couchbase, Inc All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
## \defgroup blobs Blobs
##     A \ref CBLBlob is a binary data blob associated with a document.
##
##     The content of the blob is not stored in the document, but externally in the database.
##     It is loaded only on demand, and can be streamed. Blobs can be arbitrarily large, although
##     Sync Gateway will only accept blobs under 20MB.
##
##     The document contains only a blob reference: a dictionary with the special marker property
##     blob's data. This digest is used as the key to retrieve the blob data.
##     The dictionary usually also has the property `length`, containing the blob's length in bytes,
##     and it may have the property `content_type`, containing a MIME type.
##
##     A \ref CBLBlob object acts as a proxy for such a dictionary in a \ref CBLDocument. Once
##     you've loaded a document and located the \ref FLDict holding the blob reference, call
##     \ref CBLBlob_Get on it to create a \ref CBLBlob object you can call.
##     The object has accessors for the blob's metadata and for loading the data itself.
##
##     To create a new blob from in-memory data, call \ref CBLBlob_CreateWithData, then call
##     \ref FLMutableDict_SetBlob or \ref FLMutableArray_SetBlob to add the \ref CBLBlob to the
##     document (or to a dictionary or array property of the document.)
##
##     To create a new blob from a stream, call \ref CBLBlobWriter_New to create a
##     \ref CBLBlobWriteStream, then make one or more calls to \ref CBLBlobWriter_Write to write
##     data to the blob, then finally call \ref CBLBlob_CreateWithStream to create the blob.
##     To store the blob into a document, do as in the previous paragraph.
##
##
var kTypeProperty* {.importc: "kCBLTypeProperty".}: FLSlice


var kBlobType* {.importc: "kCBLBlobType".}: FLSlice

## `"blob"`
var kBlobDigestProperty* {.importc: "kCBLBlobDigestProperty".}: FLSlice

## `"digest"`
var kBlobLengthProperty* {.importc: "kCBLBlobLengthProperty".}: FLSlice

## `"length"`
var kBlobContentTypeProperty* {.importc: "kCBLBlobContentTypeProperty".}: FLSlice

## `"content_type"`
## Returns true if a dictionary in a document is a blob reference.
##         If so, you can call \ref CBLBlob_Get to access it.
##                 whose value is `"blob"`.
proc isBlob*(a1: FLDict): bool {.importc: "CBL_IsBlob".}

## Returns a CBLBlob object corresponding to a blob dictionary in a document.
proc getBlob*(blobDict: FLDict): CBLBlob {.importc: "CBLBlob_Get".}

## Returns the length in bytes of a blob's content (from its `length` property).
proc length*(a1: CBLBlob): uint64 {.importc: "CBLBlob_Length".}

## Returns the cryptographic digest of a blob's content (from its `digest` property).
proc digest*(a1: CBLBlob): cstring {.importc: "CBLBlob_Digest".}

## Returns a blob's MIME type, if its metadata has a `content_type` property.
proc contentType*(a1: CBLBlob): cstring {.importc: "CBLBlob_ContentType".}

## Returns a blob's metadata. This includes the `digest`, `length` and `content_type`
##         properties, as well as any custom ones that may have been added.
proc properties*(a1: CBLBlob): FLDict {.importc: "CBLBlob_Properties".}

## Reads the blob's contents into memory and returns them.
##         You are responsible for calling \ref FLFLSliceResult_Release on the returned data when done.
proc loadContent*(a1: CBLBlob; err: var CBLError): FLSliceResult {.
    importc: "CBLBlob_LoadContent".}

## A stream for reading a blob's content.
type
  BlobReadStream* = ptr object

## Opens a stream for reading a blob's content.
proc openContentStream*(a1: CBLBlob; err: var CBLError): ptr BlobReadStream {.
    importc: "CBLBlob_OpenContentStream".}

## Reads data from a blob.
proc read*(stream: ptr BlobReadStream; dst: pointer; maxLength: csize_t;
    err: var CBLError): cint {.importc: "CBLBlobReader_Read".}

## Closes a CBLBlobReadStream.
proc close*(a1: ptr BlobReadStream) {.importc: "CBLBlobReader_Close".}

## Creates a new blob given its contents as a single block of data.
##                 has been saved.
proc createBlobWithData*(contentType: cstring; contents: FLSlice): CBLBlob {.
    importc: "CBLBlob_CreateWithData".}

## A stream for writing a new blob to the database.
type
  BlobWriteStream* = ptr object

## Opens a stream for writing a new blob.
##         You should next call \ref CBLBlobWriter_Write one or more times to write the data,
##         then \ref CBLBlob_CreateWithStream to create the blob.
##
##         If for some reason you need to abort, just call \ref CBLBlobWriter_Close.
proc newBlobWriter*(db: CBLDatabase; err: var CBLError): ptr BlobWriteStream {.
    importc: "CBLBlobWriter_New".}

## Closes a blob-writing stream, if you need to give up without creating a \ref CBLBlob.
proc close*(a1: ptr BlobWriteStream) {.importc: "CBLBlobWriter_Close".}

## Writes data to a new blob.
proc write*(writer: ptr BlobWriteStream; data: pointer; length: csize_t;
    err: var CBLError): bool {.importc: "CBLBlobWriter_Write".}

## Creates a new blob after its data has been written to a \ref CBLBlobWriteStream.
##         You should then add the blob to a mutable document as a property -- see
##         \ref FLMutableDict_SetBlob and \ref FLMutableArray_SetBlob.
proc createBlobWithStream*(contentType: cstring;
    writer: ptr BlobWriteStream): CBLBlob {.
    importc: "CBLBlob_CreateWithStream".}
proc createBlobWithStream_s*(contentType: cstring;
    writer: ptr BlobWriteStream): CBLBlob {.
    importc: "CBLBlob_CreateWithStream".}

## Returns true if a value in a document is a blob reference.
##         If so, you can call \ref FLValue_GetBlob to access it.
proc isBlob*(v: FLValue): bool {.inline.} =
  return isBlob(asDict(v))

## Instantiates a \ref CBLBlob object corresponding to a blob dictionary in a document.
proc getBlob*(value: FLValue): CBLBlob {.inline.} =
  return getBlob(asDict(value))

## Stores a blob in a mutable array or dictionary.
proc setBlob*(slot: FLSlot; blob: CBLBlob) {.importc: "FLSlot_SetBlob".}




##
##  CBLQuery.h
##
##  Copyright (c) 2018 Couchbase, Inc All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
## \defgroup queries   Queries
##     A CBLQuery represents a compiled database query. The query language is a large subset of
##     the [N1QL](https://www.couchbase.com/products/n1ql) language from Couchbase Server, which
##     you can think of as "SQL for JSON" or "SQL++".
##
##     Queries may be given either in
##     [N1QL syntax](https://docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/index.html),
##     or in JSON using a
##     [schema](https://github.com/couchbase/couchbase-lite-core/wiki/JSON-CBLQuery-Schema)
##     that resembles a parse tree of N1QL. The JSON syntax is harder for humans, but much more
##     amenable to machine generation, if you need to create queries programmatically or translate
##     them from some other form.
##
## CBLQuery languages
type
  CBLQueryLanguage* {.size: sizeof(cint).} = enum
    kJSONLanguage, ## [JSON query schema](https://github.com/couchbase/couchbase-lite-core/wiki/JSON-CBLQuery-Schema)
    kN1QLLanguage ## [N1QL syntax](https://docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/index.html)


## \name  CBLQuery objects
## Creates a new query by compiling the input string.
##     This is fast, but not instantaneous. If you need to run the same query many times, keep the
##     \ref CBLQuery around instead of compiling it each time. If you need to run related queries
##     with only some values different, create one query with placeholder parameter(s), and substitute
##     the desired value(s) with \ref CBLQuery_SetParameters each time you run the query.
##             [JSON](https://github.com/couchbase/couchbase-lite-core/wiki/JSON-CBLQuery-Schema) or
##             [N1QL](https://docs.couchbase.com/server/4.0/n1ql/n1ql-language-reference/index.html).
##                     input expression will be stored here (or -1 if not known/applicable.)
proc newQuery*(db: CBLDatabase; language: CBLQueryLanguage;
    queryFLString: cstring; outErrorPos: ptr cint;
    err: var CBLError): CBLQuery {.importc: "CBLQuery_New".}

## Assigns values to the query's parameters.
##     These values will be substited for those parameters whenever the query is executed,
##     until they are next assigned.
##
##     Parameters are specified in the query source as
##     e.g. `$PARAM` (N1QL) or `["$PARAM"]` (JSON). In this example, the `parameters` dictionary
##     to this call should have a key `PARAM` that maps to the value of the parameter.
##             keys are the parameter names. (It's easiest to construct this by using the mutable
##             API, i.e. calling \ref FLMutableDict_New and adding keys/values.)
proc setParameters*(query: CBLQuery; parameters: FLDict) {.
    importc: "CBLQuery_SetParameters".}

## Returns the query's current parameter bindings, if any.
proc parameters*(query: CBLQuery): FLDict {.importc: "CBLQuery_Parameters".}

## Assigns values to the query's parameters, from JSON data.
##     See \ref CBLQuery_SetParameters for details.
##             keys are the parameter names. (You may use JSON5 syntax.)
proc setParametersAsJSON*(query: CBLQuery; json: cstring): bool {.
    importc: "CBLQuery_SetParametersAsJSON".}

## Runs the query, returning the results.
##     To obtain the results you'll typically call \ref CBLResultSet_Next in a `while` loop,
##     examining the values in the \ref CBLResultSet each time around.
proc execute*(a1: CBLQuery; err: var CBLError): CBLResultSet {.
    importc: "CBLQuery_Execute".}

## Returns information about the query, including the translated SQLite form, and the search
##     strategy. You can use this to help optimize the query: the word `SCAN` in the strategy
##     indicates a linear scan of the entire database, which should be avoided by adding an index.
##     The strategy will also show which index(es), if any, are used.
proc explain*(a1: CBLQuery): FLSliceResult {.importc: "CBLQuery_Explain".}

## Returns the number of columns in each result.
proc columnCount*(a1: CBLQuery): cuint {.importc: "CBLQuery_ColumnCount".}

## Returns the name of a column in the result.
##     The column name is based on its expression in the `SELECT...` or `WHAT:` section of the
##     query. A column that returns a property or property path will be named after that property.
##     A column that returns an expression will have an automatically-generated name like `$1`.
##     To give a column a custom name, use the `AS` syntax in the query.
##     Every column is guaranteed to have a unique name.
proc columnName*(a1: CBLQuery; columnIndex: cuint): FLSlice {.
    importc: "CBLQuery_ColumnName".}

## \name  Result sets
##     A `CBLResultSet` is an iterator over the results returned by a query. It exposes one
##     result at a time -- as a collection of values indexed either by position or by name --
##     and can be stepped from one result to the next.
##
##     It's important to note that the initial position of the iterator is _before_ the first
##     result, so \ref CBLResultSet_Next must be called _first_. Example:
##
##     ```
##     CBLResultSet *rs = CBLQuery_Execute(query, &error);
##     assert(rs);
##     while (CBLResultSet_Next(rs) {
##         FLValue aValue = CBLResultSet_ValueAtIndex(rs, 0);
##         ...
##     }
##     CBLResultSet_Release(rs);
##     ```
##
## Moves the result-set iterator to the next result.
##     Returns false if there are no more results.
proc next*(a1: CBLResultSet): bool {.importc: "CBLResultSet_Next".}

## Returns the value of a column of the current result, given its (zero-based) numeric index.
##     This may return a NULL pointer, indicating `MISSING`, if the value doesn't exist, e.g. if
##     the column is a property that doesn't exist in the document.
proc valueAtIndex*(a1: CBLResultSet; index: cuint): FLValue {.
    importc: "CBLResultSet_ValueAtIndex".}

## Returns the value of a column of the current result, given its name.
##     This may return a NULL pointer, indicating `MISSING`, if the value doesn't exist, e.g. if
##     the column is a property that doesn't exist in the document. (Or, of course, if the key
##     is not a column name in this query.)
proc valueForKey*(a1: CBLResultSet; key: cstring): FLValue {.
    importc: "CBLResultSet_ValueForKey".}

## Returns the current result as an array of column values.
##    @warning The array reference is only valid until the result-set is advanced or released.
##            If you want to keep it for longer, call \ref FLArray_Retain (and release it when done.)
proc rowArray*(rs: CBLResultSet): FLArray {.importc: "CBLResultSet_RowArray".}

## Returns the current result as a dictionary mapping column names to values.
##    @warning The dict reference is only valid until the result-set is advanced or released.
##            If you want to keep it for longer, call \ref FLDict_Retain (and release it when done.)
proc rowDict*(rs: CBLResultSet): FLDict {.importc: "CBLResultSet_RowDict".}

## Returns the CBLQuery that created this CBLResultSet.
proc getQuery*(rs: CBLResultSet): CBLQuery {.importc: "CBLResultSet_GetQuery".}

## \name  Change listener
##     Adding a change listener to a query turns it into a "live query". When changes are made to
##     documents, the query will periodically re-run and compare its results with the prior
##     results; if the new results are different, the listener callback will be called.
##
##             rows that changed.
##
## A callback to be invoked after the query's results have changed.
##     The actual result set can be obtained by calling \ref CBLQuery_CurrentResults, either during
##     the callback or at any time thereafter.
##                     prepared for that, you may want to use \ref CBLDatabase_BufferNotifications
##                     so that listeners will be called in a safe context.
type
  CBLQueryChangeListner* = proc (context: pointer; query: CBLQuery)

## Registers a change listener callback with a query, turning it into a "live query" until
##     the listener is removed (via \ref CBLListener_Remove).
##
##     When the first change listener is added, the query will run (in the background) and notify
##     the listener(s) of the results when ready. After that, it will run in the background after
##     the database changes, and only notify the listeners when the result set changes.
##             listener.
proc addChangeListener*(query: CBLQuery; listener: CBLQueryChangeListner;
    context: pointer): CBLListenerToken {.
    importc: "CBLQuery_AddChangeListener".}

## Returns the query's _entire_ current result set, after it's been announced via a call to the
##     listener's callback.
proc copyCurrentResults*(query: CBLQuery; listener: CBLListenerToken;
    err: var CBLError): CBLResultSet {.importc: "CBLQuery_CopyCurrentResults".}

## \name  CBLDatabase Indexes
##     Indexes are used to speed up queries by allowing fast -- O(log n) -- lookup of documents
##     that have specific values or ranges of values. The values may be properties, or expressions
##     based on properties.
##
##     An index will speed up queries that use the expression it indexes, but it takes up space in
##     the database file, and it slows down document saves slightly because it needs to be kept up
##     to date when documents change.
##
##     Tuning a database with indexes can be a tricky task. Fortunately, a lot has been written about
##     it in the relational-database (SQL) realm, and much of that advice holds for Couchbase Lite.
##     You may find SQLite's documentation particularly helpful since Couchbase Lite's querying is
##     based on SQLite.
##
##     Two types of indexes are currently supported:
##  FLValue indexes speed up queries by making it possible to look up property (or expression)
##           values without scanning every document. They're just like regular indexes in SQL or N1QL.
##           Multiple expressions are supported; the first is the primary key, second is secondary.
##           Expressions must evaluate to scalar types (boolean, number, string).
##  Full-Text Search (FTS) indexes enable fast search of natural-language words or phrases
##           by using the `MATCH` operator in a query. A FTS index is **required** for full-text
##           search: a query with a `MATCH` operator will fail to compile unless there is already a
##           FTS index for the property/expression being matched. Only a single expression is
##           currently allowed, and it must evaluate to a string.
## Types of database indexes.
type
  CBLIndexType* {.size: sizeof(cint).} = enum
    kValueIndex, ## An index that stores property or expression values
    kFullTextIndex ## An index of strings, that enables searching for words with `MATCH`


## Parameters for creating a database index.
type
  CBLIndexSpec* {.bycopy.} = object
    `type`*: CBLIndexType ## The type of index to create.
                          ## A JSON array describing each column of the index.
    keyExpressionsJSON*: cstring ## In a full-text index, should diacritical marks (accents) be ignored?
                          ##         Defaults to false. Generally this should be left `false` for non-English text.
    ignoreAccents*: bool ## In a full-text index, the dominant language. Setting this enables word stemming, i.e.
                          ##         matching different cases of the same word ("big" and "bigger", for instance) and ignoring
                          ##         common "stop-words" ("the", "a", "of", etc.)
                          ##
                          ##         Can be an ISO-639 language code or a lowercase (English) language name; supported
                          ##         languages are: da/danish, nl/dutch, en/english, fi/finnish, fr/french, de/german,
                          ##         hu/hungarian, it/italian, no/norwegian, pt/portuguese, ro/romanian, ru/russian,
                          ##         es/spanish, sv/swedish, tr/turkish.
                          ##
                          ##         If left null,  or set to an unrecognized language, no language-specific behaviors
                          ##         such as stemming and stop-word removal occur.
    language*: cstring


## Creates a database index.
##     Indexes are persistent.
##     If an identical index with that name already exists, nothing happens (and no error is returned.)
##     If a non-identical index with that name already exists, it is deleted and re-created.
proc createDatabaseIndex*(db: CBLDatabase; name: cstring; a3: CBLIndexSpec;
    err: var CBLError): bool {.importc: "CBLDatabase_CreateIndex".}

## Deletes an index given its name.
proc deleteIndex*(db: CBLDatabase; name: cstring; err: var CBLError): bool {.
    importc: "CBLDatabase_DeleteIndex".}

## Returns the names of the indexes on this database, as an array of strings.
proc indexNames*(db: CBLDatabase): FLMutableArray {.
    importc: "CBLDatabase_IndexNames".}




##
##  CBLReplicator.h
##
##  Copyright (c) 2018 Couchbase, Inc All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
## \defgroup replication   Replication
##     A replicator is a background task that synchronizes changes between a local database and
##     another database on a remote server (or on a peer device, or even another local database.)
## \name  Configuration
## The name of the HTTP cookie used by Sync Gateway to store session keys.
var kAuthDefaultCookieName* {.importc: "kCBLAuthDefaultCookieName".}: cstring

type
  CBLEndpoint* = ptr object
  CBLAuthenticator* = ptr object ## An opaque object representing authentication credentials for a remote server.

## Creates a new endpoint representing a server-based database at the given URL.
##     The URL's scheme must be `ws` or `wss`, it must of course have a valid hostname,
##     and its path must be the name of the database on that server.
##     The port can be omitted; it defaults to 80 for `ws` and 443 for `wss`.
##     For example: `wss://example.org/dbname`
proc newEndpointWithURL*(url: cstring): CBLEndpoint {.
    importc: "CBLEndpoint_NewWithURL".}

## Creates a new endpoint representing another local database. (Enterprise Edition only.)
proc newEndpointWithLocalDB*(db: CBLDatabase): CBLEndpoint {.
    importc: "CBLEndpoint_NewWithLocalDB".}

## Frees a CBLEndpoint object.
proc free*(a1: CBLEndpoint) {.importc: "CBLEndpoint_Free".}


## Creates an authenticator for HTTP Basic (username/password) auth.
proc newAuthBasic*(username: cstring; password: cstring): CBLAuthenticator {.
    importc: "CBLAuth_NewBasic".}

## Creates an authenticator using a Couchbase Sync Gateway login session identifier,
##     and optionally a cookie name (pass NULL for the default.)
proc newAuthSession*(sessionID: cstring;
    cookieName: cstring): CBLAuthenticator {.importc: "CBLAuth_NewSession".}

## Frees a CBLAuthenticator object.
proc free*(a1: CBLAuthenticator) {.importc: "CBLAuth_Free".}

## Direction of replication: push, pull, or both.
type
  CBLReplicatorType* {.size: sizeof(cint).} = enum
    kReplicatorTypePushAndPull = 0, ## Bidirectional; both push and pull
    kReplicatorTypePush,            ## Pushing changes to the target
    kReplicatorTypePull             ## Pulling changes from the target


## A callback that can decide whether a particular document should be pushed or pulled.
##                 It must pay attention to thread-safety. It should not take a long time to return,
##                 or it will slow down the replicator.
type
  CBLReplicationFilter* = proc (context: pointer; document: CBLDocument;
      isDeleted: bool): bool

## Conflict-resolution callback for use in replications. This callback will be invoked
##     when the replicator finds a newer server-side revision of a document that also has local
##     changes. The local and remote changes must be resolved before the document can be pushed
##     to the server.
##                 It must pay attention to thread-safety. However, unlike a filter callback,
##                 it does not need to return quickly. If it needs to prompt for user input,
##                 that's OK.
##                 or NULL if the local document has been deleted.
##                 or NULL if the document has been deleted on the server.
##         This can be the same as \p localDocument or \p remoteDocument, or you can create
##         a mutable copy of either one and modify it appropriately.
##         Or return NULL if the resolution is to delete the document.
type
  CBLConflictResolver* = proc (context: pointer; documentID: cstring;
      localDocument: CBLDocument; remoteDocument: CBLDocument): CBLDocument

## Default conflict resolver. This always returns `localDocument`.
var defaultConflictResolver* {.importc: "CBLDefaultConflictResolver".}: CBLConflictResolver

## Types of proxy servers, for CBLProxySettings.
type
  CBLProxyType* {.size: sizeof(cint).} = enum
    kProxyHTTP, ## HTTP proxy; must support 'CONNECT' method
    kProxyHTTPS ## HTTPS proxy; must support 'CONNECT' method


## Proxy settings for the replicator.
type
  CBLProxySettings* {.bycopy.} = object
    `type`*: CBLProxyType ## Type of proxy
    hostname*: cstring    ## Proxy server hostname or IP address
    port*: uint16         ## Proxy server port
    username*: cstring    ## Username for proxy auth (optional)
    password*: cstring    ## Password for proxy auth


## The configuration of a replicator.
type
  CBLReplicatorConfiguration* {.bycopy.} = object
    database*: CBLDatabase                 ## The database to replicate
    endpoint*: CBLEndpoint ## The address of the other database to replicate with
    replicatorType*: CBLReplicatorType     ## Push, pull or both
    continuous*: bool                      ## Continuous replication?
                                           ## -- HTTP settings:
    authenticator*: CBLAuthenticator       ## Authentication credentials, if needed
    proxy*: ptr CBLProxySettings           ## HTTP client proxy settings
    headers*: FLDict ## Extra HTTP headers to add to the WebSocket request
                                           ## -- TLS settings:
    pinnedServerCertificate*: FLSlice ## An X.509 cert to "pin" TLS connections to (PEM or DER)
    trustedRootCertificates*: FLSlice      ## Set of anchor certs (PEM format)
                                           ## -- Filtering:
    channels*: FLArray                     ## Optional set of channels to pull from
    documentIDs*: FLArray                  ## Optional set of document IDs to replicate
    pushFilter*: CBLReplicationFilter      ## Optional callback to filter which docs are pushed
    pullFilter*: CBLReplicationFilter      ## Optional callback to validate incoming docs
    conflictResolver*: CBLConflictResolver ## Optional conflict-resolver callback
    context*: pointer                      ## Arbitrary value that will be passed to callbacks


## \name  Lifecycle
## Creates a replicator with the given configuration.
proc newReplicator*(a1: ptr CBLReplicatorConfiguration;
    err: var CBLError): CBLReplicator {.importc: "CBLReplicator_New".}

## Returns the configuration of an existing replicator.
proc config*(a1: CBLReplicator): ptr CBLReplicatorConfiguration {.
    importc: "CBLReplicator_Config".}

## Instructs the replicator to ignore existing checkpoints the next time it runs.
##     This will cause it to scan through all the documents on the remote database, which takes
##     a lot longer, but it can resolve problems with missing documents if the client and
##     server have gotten out of sync somehow.
proc resetCheckpoint*(a1: CBLReplicator) {.
    importc: "CBLReplicator_ResetCheckpoint".}

## Starts a replicator, asynchronously. Does nothing if it's already started.
proc start*(a1: CBLReplicator) {.importc: "CBLReplicator_Start".}

## Stops a running replicator, asynchronously. Does nothing if it's not already started.
##     The replicator will call your \ref CBLReplicatorChangeListener with an activity level of
##     \ref kCBLReplicatorStopped after it stops. Until then, consider it still active.
proc stop*(a1: CBLReplicator) {.importc: "CBLReplicator_Stop".}

## Informs the replicator whether it's considered possible to reach the remote host with
##     the current network configuration. The default value is true. This only affects the
##     replicator's behavior while it's in the Offline state:
##  Setting it to false will cancel any pending retry and prevent future automatic retries.
##  Setting it back to true will initiate an immediate retry.
proc setHostReachable*(a1: CBLReplicator; reachable: bool) {.
    importc: "CBLReplicator_SetHostReachable".}

## Puts the replicator in or out of "suspended" state. The default is false.
##  Setting suspended=true causes the replicator to disconnect and enter Offline state;
##       it will not attempt to reconnect while it's suspended.
##  Setting suspended=false causes the replicator to attempt to reconnect, _if_ it was
##       connected when suspended, and is still in Offline state.
proc setSuspended*(repl: CBLReplicator; suspended: bool) {.
    importc: "CBLReplicator_SetSuspended".}

## \name  Status and Progress
##
## The possible states a replicator can be in during its lifecycle.
type
  CBLReplicatorActivityLevel* {.size: sizeof(cint).} = enum
    kReplicatorStopped,    ## The replicator is unstarted, finished, or hit a fatal error.
    kReplicatorOffline,    ## The replicator is offline, as the remote host is unreachable.
    kReplicatorConnecting, ## The replicator is connecting to the remote host.
    kReplicatorIdle,       ## The replicator is inactive, waiting for changes to sync.
    kReplicatorBusy        ## The replicator is actively transferring data.


## A fractional progress value, ranging from 0.0 to 1.0 as replication progresses.
##     The value is very approximate and may bounce around during replication; making it more
##     accurate would require slowing down the replicator and incurring more load on the server.
##     It's fine to use in a progress bar, though.
type
  CBLReplicatorProgress* {.bycopy.} = object
    fractionComplete*: cfloat ## / Very-approximate completion, from 0.0 to 1.0
    documentCount*: uint64    ## Number of documents transferred so far


## A replicator's current status.
type
  ReplicatorStatus* {.bycopy.} = object
    activity*: CBLReplicatorActivityLevel ## Current state
    progress*: CBLReplicatorProgress      ## Approximate fraction complete
    error*: CBLError                      ## CBLError, if any


## Returns the replicator's current status.
proc getStatus*(a1: CBLReplicator): ReplicatorStatus {.
    importc: "CBLReplicator_GetStatus".}

## Indicates which documents have local changes that have not yet been pushed to the server
##     by this replicator. This is of course a snapshot, that will go out of date as the replicator
##     makes progress and/or documents are saved locally.
##
##     The result is, effectively, a set of document IDs: a dictionary whose keys are the IDs and
##     values are `true`.
##     If there are no pending documents, the dictionary is empty.
##     On error, NULL is returned.
##
##     \note  This function can be called on a stopped or un-started replicator.
##     \note  Documents that would never be pushed by this replicator, due to its configuration's
##            `pushFilter` or `docIDs`, are ignored.
##     \warning  You are responsible for releasing the returned array via \ref FLValue_Release.
proc pendingDocumentIDs*(a1: CBLReplicator; err: var CBLError): FLDict {.
    importc: "CBLReplicator_PendingDocumentIDs".}

## Indicates whether the document with the given ID has local changes that have not yet been
##     pushed to the server by this replicator.
##
##     This is equivalent to, but faster than, calling \ref CBLReplicator_PendingDocumentIDs and
##     checking whether the result contains \p docID. See that function's documentation for details.
##
##     \note  A `false` result means the document is not pending, _or_ there was an error.
##            To tell the difference, compare the error code to zero.
proc isDocumentPending*(repl: CBLReplicator; docID: FLString;
    err: var CBLError): bool {.importc: "CBLReplicator_IsDocumentPending".}

## A callback that notifies you when the replicator's status changes.
##                 It must pay attention to thread-safety. It should not take a long time to return,
##                 or it will slow down the replicator.
type
  CBLReplicatorChangeListener* = proc (context: pointer;
      replicator: CBLReplicator; status: ptr ReplicatorStatus)

## Adds a listener that will be called when the replicator's status changes.
proc addChangeListener*(a1: CBLReplicator; a2: CBLReplicatorChangeListener;
    context: pointer): CBLListenerToken {.
    importc: "CBLReplicator_AddChangeListener".}

## Flags describing a replicated document.
type
  CBLDocumentFlags* {.size: sizeof(cint).} = enum
    kDocumentFlagsDeleted = 1 shl 0, ## The document has been deleted.
    kDocumentFlagsAccessRemoved = 1 shl 1


## Information about a document that's been pushed or pulled.
type
  CBLReplicatedDocument* {.bycopy.} = object
    id*: cstring     ## The document ID
    flags*: CBLDocumentFlags ## Indicates whether the document was deleted or removed
    error*: CBLError ## If the code is nonzero, the document failed to replicate.


## A callback that notifies you when documents are replicated.
##                 It must pay attention to thread-safety. It should not take a long time to return,
##                 or it will slow down the replicator.
type
  CBLReplicatedDocumentListener* = proc (context: pointer;
      replicator: CBLReplicator; isPush: bool; numDocuments: cuint;
      documents: ptr CBLReplicatedDocument)

## Adds a listener that will be called when documents are replicated.
proc addDocumentListener*(a1: CBLReplicator; a2: CBLReplicatedDocumentListener;
    context: pointer): CBLListenerToken {.
    importc: "CBLReplicator_AddDocumentListener".}

{.pop.}
