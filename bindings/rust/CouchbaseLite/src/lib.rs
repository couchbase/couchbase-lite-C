// mod couchbase_lite

//#![allow(unused_imports)]
//#![allow(dead_code)]

#[macro_use] extern crate enum_primitive;

pub mod database;
pub mod document;
pub mod error;
pub mod fleece;
pub mod fleece_mutable;
pub mod logging;
pub mod query;

mod slice;
mod c_api;

mod fleece_tests;

use self::c_api::*;


//////// RE-EXPORT:


pub use error::*;
pub use fleece::*;
pub use fleece_mutable::*;
pub use query::*;


//////// TOP-LEVEL TYPES:


pub struct Timestamp(i64);


pub struct ListenerToken {
    _ref: *mut CBLListenerToken
}


impl Drop for ListenerToken {
    fn drop(&mut self) {
        unsafe { CBLListener_Remove(self._ref) }
    }
}


//////// MISC. API FUNCTIONS


pub fn instance_count() -> usize {
    unsafe { return CBL_InstanceCount() as usize }
}

pub fn dump_instances() {
    unsafe { CBL_DumpInstances() }
}


//////// DATABASE:


// Database configuration flags:
pub static CREATE     : u32 = kCBLDatabase_Create;
pub static READ_ONLY  : u32 = kCBLDatabase_ReadOnly;
pub static NO_UPGRADE : u32 = kCBLDatabase_NoUpgrade;


pub struct DatabaseConfiguration<'a> {
    pub directory:  &'a std::path::Path,
    pub flags:      u32
}


pub struct Database {
    _ref: *mut CBLDatabase
}


//////// DOCUMENT:


pub enum ConcurrencyControl {
    LastWriteWins  = kCBLConcurrencyControlLastWriteWins as isize,
    FailOnConflict = kCBLConcurrencyControlFailOnConflict as isize
}


pub struct Document {
    _ref: *mut CBLDocument
}


//////// REFCOUNT SUPPORT


pub(crate) unsafe fn retain<T>(cbl_ref: *mut T) -> *mut T {
    return CBL_Retain(cbl_ref as *mut CBLRefCounted) as *mut T
}

pub(crate) unsafe fn release<T>(cbl_ref: *mut T) {
    CBL_Release(cbl_ref as *mut CBLRefCounted)
}
