// mod couchbase_lite

// TODO: Re-enable these warnings once I'm further along
#![allow(unused_imports)]
#![allow(dead_code)]

#[macro_use] extern crate enum_primitive;

pub mod fleece;
pub mod fleece_mutable;
pub mod database;
pub mod document;
pub mod error;
pub mod logging;

mod base;
mod c_api;

mod fleece_tests;

use self::base::*;
use self::c_api::*;


//////// RE-EXPORT:

pub use error::*;
pub use fleece::*;
pub use fleece_mutable::*;


//////// TOP-LEVEL NAMESPACE:


pub struct Timestamp(i64);


pub struct ListenerToken {
    _ref: *mut CBLListenerToken
}


//////// OTHER FUNCTIONS


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
