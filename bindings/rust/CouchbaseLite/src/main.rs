#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

use std::ffi::c_void;
use std::ptr;

fn as_slice(s: &str) -> FLSlice {
    return FLSlice{buf: s.as_ptr() as *const c_void, size: s.len() as u64};
}

unsafe fn open(name: &str, dir: &str) -> *mut CBLDatabase {
    let config = CBLDatabaseConfiguration_s {
        directory:     as_slice(dir),
        flags:         kCBLDatabase_Create,
        encryptionKey: ptr::null_mut()
    };
    let mut err = CBLError{domain: 0, code: 0, internal_info: 0};
    return CBLDatabase_Open_s(as_slice(name), &config, &mut err);
}

unsafe fn close(db: *mut CBLDatabase) {
    CBL_Release(db as *mut CBLRefCounted);
}


fn main() {
    unsafe {
        println!("Hello, world!");
        let db = open("rusty", "/tmp");
        println!("Closing database; goodbye, world!");
        close(db);
    }
}
