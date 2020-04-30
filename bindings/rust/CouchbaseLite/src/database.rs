// mod database

use super::*;
use super::base::*;
use super::error::*;
use super::c_api::*;

use std::path::*;
use std::ptr;


type ChangeListener = fn(db: &Database, doc_ids: Vec<String>);


impl Database {
    
    //////// CONSTRUCTORS:
    
    
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
    
    
    pub fn open_in_dir<P: AsRef<Path>>(name: &str, dir: P) -> Result<Database> {
        let config = DatabaseConfiguration{directory: dir.as_ref(), flags: CREATE};
        return Database::open(name, Some(config));
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
    
    
    pub fn exists<P: AsRef<Path>>(name: &str, in_directory: P) -> bool {
        unsafe {
            return CBL_DatabaseExists_s(as_slice(name), 
                                        as_slice(in_directory.as_ref().to_str().unwrap()));
        }
    }
    
    
    // Deletes an unopened database file. Returns true on delete, false on no file, or error.
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
       
       
    // Closes & deletes a database. (Note it consumes `self`!)
    pub fn delete(self) -> Result<()> {
        unsafe {
            return check_bool(|error| CBLDatabase_Delete(self._ref, error));
        }
    }
    
    
    pub fn config(&self) -> DatabaseConfiguration {
        todo!()
    }
    
    
    //////// ACCESSORS:
    
    
    pub fn name(&self) -> String {
        unsafe {
            return to_string(CBLDatabase_Name(self._ref));
        }
    }
    
    
    pub fn path(&self) -> PathBuf {
        unsafe {
            return PathBuf::from(to_string(CBLDatabase_Path(self._ref)));
        }
    }
    
    
    pub fn count(&self) -> u64 {
        unsafe {
            return CBLDatabase_Count(self._ref);
        }
    }


    //////// OTHER OPERATIONS:
    

    pub fn compact(&self) -> Result<()> {
        unsafe {
            return check_bool(|error| CBLDatabase_Compact(self._ref, error));
        }
    }
    
    
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
    
    
    //////// NOTIFICATIONS:
    
    
    pub fn add_listener(&self, _listener: ChangeListener) -> ListenerToken {
        todo!()
    }
    
    pub fn buffer_notifications(&self, _callback: fn(&Database)) {
        todo!()
    }
    
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
