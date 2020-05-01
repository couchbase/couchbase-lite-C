// mod database

use super::*;
use super::slice::*;
use super::error::*;
use super::c_api::*;

use std::path::*;
use std::ptr;


// Database configuration flags:
pub static CREATE     : u32 = kCBLDatabase_Create;
pub static READ_ONLY  : u32 = kCBLDatabase_ReadOnly;
pub static NO_UPGRADE : u32 = kCBLDatabase_NoUpgrade;


/** Database configuration options. */
pub struct DatabaseConfiguration<'a> {
    pub directory:  &'a std::path::Path,
    pub flags:      u32
}


type ChangeListener = fn(db: &Database, doc_ids: Vec<String>);


/** A connection to an open database. */
pub struct Database {
    pub(crate) _ref: *mut CBLDatabase
}


impl Database {

    //////// CONSTRUCTORS:


    /** Opens a database, or creates it if it doesn't exist yet, returning a new \ref CBLDatabase
        instance.
        It's OK to open the same database file multiple times. Each \ref CBLDatabase instance is
        independent of the others (and must be separately closed and released.) */
    pub fn open(name: &str, config: Option<DatabaseConfiguration>) -> Result<Database> {
        unsafe {
            if let Some(cfg) = config {
                let c_config = CBLDatabaseConfiguration_s {
                    directory:     as_slice(cfg.directory.to_str().unwrap()),
                    flags:         cfg.flags,
                    encryptionKey: ptr::null_mut() // TODO
                };
                return Database::_open(name, &c_config);
            } else {
                return Database::_open(name, ptr::null())
            }
        }
    }


    unsafe fn _open(name: &str, config_ptr: *const CBLDatabaseConfiguration_s) -> Result<Database> {
        let mut err = CBLError::default();
        let db_ref = CBLDatabase_Open_s(as_slice(name), config_ptr, &mut err);
        if db_ref.is_null() {
            return failure(err);
        }
        return Ok(Database{_ref: db_ref});
    }


    //////// OTHER STATIC METHODS:


    /** Returns true if a database with the given name exists in the given directory. */
    pub fn exists<P: AsRef<Path>>(name: &str, in_directory: P) -> bool {
        unsafe {
            return CBL_DatabaseExists_s(as_slice(name),
                                        as_slice(in_directory.as_ref().to_str().unwrap()));
        }
    }


    /** Deletes a database file. If the database file is open, an error is returned. */
    pub fn delete_file<P: AsRef<Path>>(name: &str, in_directory: P) -> Result<bool> {
        unsafe {
            let mut error = CBLError::default();
            if CBL_DeleteDatabase_s(as_slice(name),
                                    as_slice(in_directory.as_ref().to_str().unwrap()),
                                    &mut error) {
                return Ok(true);
            } else if !error {
                return Ok(false);
            } else {
                return failure(error);
            }
        }
    }


    //////// OPERATIONS:


    /** Closes and deletes a database. If there are any other connections to the database,
        an error is returned. */
    pub fn delete(self) -> Result<()> {
        unsafe { check_bool(|error| CBLDatabase_Delete(self._ref, error)) }
    }


    /** Compacts a database file, freeing up unused disk space. */
    pub fn compact(&self) -> Result<()> {
        unsafe {
            return check_bool(|error| CBLDatabase_Compact(self._ref, error));
        }
    }


     /** Invokes the callback as a batch operation, similar to a transaction.
         - Multiple writes are much faster when grouped inside a single batch.
         - Changes will not be visible to other CBLDatabase instances on the same database until
                the batch operation ends.
         - Batch operations can nest. Changes are not committed until the outer batch ends. */
   pub fn in_batch<T>(&self, callback: fn()->T) -> Result<T> {
        let mut err = CBLError::default();
        unsafe {
            if ! CBLDatabase_BeginBatch(self._ref, &mut err) {
                return failure(err);
            }
        }
        let result = callback();
        unsafe {
            if ! CBLDatabase_EndBatch(self._ref, &mut err) {
                return failure(err);
            }
        }
        return Ok(result);
    }


    //////// ACCESSORS:


    /** Returns the database's name. */
    pub fn name(&self) -> String {
        unsafe {
            return to_string(CBLDatabase_Name(self._ref));
        }
    }


    /** Returns the database's full filesystem path. */
    pub fn path(&self) -> PathBuf {
        unsafe {
            return PathBuf::from(to_string(CBLDatabase_Path(self._ref)));
        }
    }


    /** Returns the number of documents in the database. */
   pub fn count(&self) -> u64 {
        unsafe {
            return CBLDatabase_Count(self._ref);
        }
    }


    //////// NOTIFICATIONS:


    /** Registers a database change listener function. It will be called after one or more
        documents are changed on disk. */
    pub fn add_listener(&self, _listener: ChangeListener) -> ListenerToken {
        todo!()
    }

    /** Switches the database to buffered-notification mode. Notifications for objects belonging
        to this database (documents, queries, replicators, and of course the database) will not be
        called immediately; your callback function will be called instead. You can then call
        \ref send_notifications when you're ready. */
    pub fn buffer_notifications(&self, _callback: fn(&Database)) {
        todo!()
    }

    /** Immediately issues all pending notifications for this database, by calling their listener
        callbacks. (Only useful after \ref buffer_notifications has been called.) */
   pub fn send_notifications(&self) {
        unsafe {
            CBLDatabase_SendNotifications(self._ref);
        }
    }

}


impl Drop for Database {
    fn drop(&mut self) {
        unsafe {
            release(self._ref)
        }
    }
}


impl Clone for Database {
    fn clone(&self) -> Self {
        unsafe {
            return Database{_ref: retain(self._ref)}
        }
    }
}
