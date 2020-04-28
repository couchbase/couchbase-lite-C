// mod base

#![allow(non_upper_case_globals)]

use super::c_api::*;

use std::borrow::Cow;
use std::ffi::c_void;
use std::ffi::CStr;
use std::ptr;
use std::str;


//////// SLICES


pub const NULL_SLICE : FLSlice = FLSlice{buf: ptr::null(), size: 0};


pub fn as_slice(s: &str) -> FLSlice {
    return FLSlice{buf: s.as_ptr() as *const c_void, 
                   size: s.len() as u64};
}

pub fn bytes_as_slice(s: &[u8]) -> FLSlice {
    return FLSlice{buf: s.as_ptr() as *const c_void, 
                   size: s.len() as u64};
}

impl FLSlice {
    // A slice may be null, so in Rust terms it's an Option.
    
    pub unsafe fn as_byte_array<'a>(&self) -> Option<&'a [u8]> {
        if !self { return None }
        return Some(std::slice::from_raw_parts(self.buf as *const u8, self.size as usize))
    }
    pub unsafe fn as_str<'a>(&self) -> Option<&'a str> {
        match self.as_byte_array() {
            None    => None,
            Some(b) => { str::from_utf8(b).ok() }
        }
    }
    pub unsafe fn to_string(&self) -> Option<String> {
        return self.as_str().map(|s| s.to_string());
    }
    
    pub fn map<F, T>(&self, f : F) -> Option<T> 
        where F: Fn(&FLSlice)->T
    {
        if !self {None} else {Some(f(self))}
    }
}

impl std::ops::Not for &FLSlice {
    type Output = bool;
    fn not(self) -> bool {self.buf.is_null()}
}

impl FLSliceResult {
    pub fn as_slice(&self) -> FLSlice {
        return FLSlice{buf: self.buf, size: self.size};
    }
    
    // pub unsafe fn retain(&mut self) {
    //     _FLBuf_Retain(self.buf);
    // }
    
    // It's not possible to implement Drop for FLSliceResult, because the generated interface
    // makes it implement Copy. So it has to be released by hand.
    pub unsafe fn release(&mut self) {
        _FLBuf_Release(self.buf);
    }
    
    // Consumes & releases self
    pub unsafe fn to_string(mut self) -> Option<String> {
        let str = self.as_slice().to_string();
        self.release();
        return str;
    }
}



//////// C STRINGS


// Convenience to convert a raw `char*` to an unowned `&str`
pub unsafe fn to_str<'a>(cstr: *const ::std::os::raw::c_char) -> Cow<'a, str> {
    return CStr::from_ptr(cstr).to_string_lossy()
}


// Convenience to convert a raw `char*` to an owned String
pub unsafe fn to_string(cstr: *const ::std::os::raw::c_char) -> String {
    return to_str(cstr).to_string();
}


//////// REFCOUNTED


pub unsafe fn retain<T>(cbl_ref: *mut T) -> *mut T {
    return CBL_Retain(cbl_ref as *mut CBLRefCounted) as *mut T
}

pub unsafe fn release<T>(cbl_ref: *mut T) {
    CBL_Release(cbl_ref as *mut CBLRefCounted)
}


//////// LISTENERS


impl Drop for super::ListenerToken {
    fn drop(&mut self) {
        unsafe {
            CBLListener_Remove(self._ref)
        }
    }
}
