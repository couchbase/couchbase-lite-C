// mod couchbase_lite

#[macro_use] extern crate enum_primitive;

pub mod fleece;
pub mod database;
pub mod document;
pub mod error;

mod base;
mod c_api;

mod fleece_tests;

use self::c_api::*;

//////// TOP-LEVEL NAMESPACE:

pub struct Timestamp(i64);


pub struct ListenerToken {
    _ref: *mut CBLListenerToken
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
