# database.nim

import CouchbaseLite/errors
import CouchbaseLite/private/cbl

import sugar


type
    DatabaseObj* = object
        handle*: cbl.Database                   # TODO: Avoid making this public
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


proc openDB(name: string; configP: ptr cbl.DatabaseConfiguration): Database =
    var err: cbl.Error
    let dbRef = cbl.openDatabase(name, configP, err)
    if dbRef == nil:
        throw(err)
    return Database(handle: dbRef)

proc openDatabase*(name: string; config: DatabaseConfiguration): Database =
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

proc inBatch*(db: Database; fn: proc()) =
    checkBool( (err) => cbl.beginBatch(db.handle, err[]) )
    defer: checkBool( (err) => cbl.endBatch(db.handle, err[]) )
    fn()

#TODO: Listeners
