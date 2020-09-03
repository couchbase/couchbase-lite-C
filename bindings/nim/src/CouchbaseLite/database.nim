# Couchbase Lite Database class
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

import CouchbaseLite/errors
import CouchbaseLite/private/cbl

import sugar

{.experimental: "notnil".}


type
  DatabaseObj* {.requiresInit.} = object
    handle: CBLDatabase not nil
  Database* = ref DatabaseObj not nil ## A connection to an open database.


proc `=destroy`(d: var DatabaseObj) =
  release(d.handle)

proc `=`(dst: var DatabaseObj, src: DatabaseObj) {.error.}


type
  DatabaseFlag* {.size: sizeof(cint).} = enum
    create,   ## Create the database if it doesn't exist
    readOnly, ## Open database read-only
    noUpgrade ## Disable upgrading an older-version database
  DatabaseFlags* = set[DatabaseFlag] ## Flags for how to open a database.

  EncryptionAlgorithm* = enum
    ## Database encryption algorithms (available only in the Enterprise
    ## Edition).
    none = 0,
    AES256
  EncryptionKey* = object
    ## Encryption key specified in a DatabaseConfiguration.
    algorithm*: EncryptionAlgorithm ## Encryption algorithm
    bytes*: array[32, uint8]        ## Raw key data

  DatabaseConfiguration* = object
    ## Database configuration options.
    directory*: string                ## The parent directory of the database
    flags*: DatabaseFlags             ## Options for opening the database
    encryptionKey*: ref EncryptionKey ## The database's encryption key (if any)


proc openDB(name: string; configP: ptr CBLDatabaseConfiguration): Database =
  var err: CBLError
  let dbRef = cbl.openDatabase(name, configP, err)
  if dbRef == nil:
    throw(err)
  else:
    return Database(handle: dbRef)

proc openDatabase*(name: string; config: DatabaseConfiguration): Database =
  ## Opens a database, or creates it if it doesn't exist yet, returning a new
  ## Database instance. The database is closed when this object is
  ## garbage-collected, or when you call ``close`` on it.It's OK to open the
  ## same database file multiple times. Each Database instance is independent
  ## of the others (and must be separately closed.)
  var cblConfig = CBLDatabaseConfiguration(
      directory: config.directory,
      flags: cast[CBLDatabaseFlags](config.flags))
  var cblKey: CBLEncryptionKey
  if config.encryptionKey != nil:
    cblKey.algorithm = cast[CBLEncryptionAlgorithm](
            config.encryptionKey.algorithm)
    cblKey.bytes = config.encryptionKey.bytes
    cblConfig.encryptionKey = addr cblKey
  return openDB(name, addr cblConfig)

proc openDatabase*(name: string): Database =
  ## Opens or creates a database, using the default configuration.
  return openDB(name, nil)

proc databaseExists*(name: string; inDirectory: string): bool =
  ## Returns true if a database with the given name exists in the given
  ## directory.
  cbl.databaseExists(name, inDirectory)

proc deleteDatabase*(name: string; inDirectory: string): bool {.discardable.} =
  ## Deletes a database file. If the database file is open, an error is
  ## returned.
  var err: CBLError
  if cbl.deleteDatabase(name, inDirectory, err):
    return true
  elif err.code == 0:
    return false
  else:
    throw(err)

proc name*(db: Database): string =
  ## The database's name (without the ``.cblite2`` extension.)
  $(db.handle.name)

proc path*(db: Database): string =
  ## The full path to the database file.
  $(db.handle.path)

proc count*(db: Database): uint64 =
  ## The number of documents in the database. (Deleted documents don't count.)
  db.handle.count

proc close*(db: Database) =
  ## Closes the database. You must not call this object any more afterwards.
  checkBool( (err) => cbl.close(db.handle, err[]))

proc delete*(db: Database) =
  ## Closes and deletes a database. If there are any other open connections to
  ## the database, an error is thrown.
  checkBool( (err) => cbl.delete(db.handle, err[]))

proc compact*(db: Database) =
  ## Compacts a database file, freeing up disk space.
  checkBool( (err) => cbl.compact(db.handle, err[]))

proc inBatch*(db: Database; fn: proc()) =
  ## Performs the callback proc within a batch operation, similar to a
  ## transaction. Multiple writes are much faster if done inside a batch.
  ## Changes wil not be visible to other Database instances until the batch
  ## operaton ends. Batch operations can nest. Changes are not committed until
  ## the outer batch ends.
  checkBool( (err) => cbl.beginBatch(db.handle, err[]))
  defer: checkBool( (err) => cbl.endBatch(db.handle, err[]))
  fn()

proc internal_handle*(db: Database): CBLDatabase =
  # INTERNAL ONLY
  db.handle

#TODO: Listeners
