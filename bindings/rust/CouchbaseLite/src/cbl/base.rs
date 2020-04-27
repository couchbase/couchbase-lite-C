// mod base

#![allow(non_upper_case_globals)]

use cbl::c_api::*;

use enum_primitive::FromPrimitive;
use std::borrow::Borrow;
use std::borrow::Cow;
use std::ffi::c_void;
use std::ffi::CStr;
use std::str;


//////// ERROR STRUCT:


enum_from_primitive! {
    #[derive(Debug, PartialEq)]
    pub enum CouchbaseLiteError {
        AssertionFailed = 1,    // Internal assertion failure
        Unimplemented,          // Oops, an unimplemented API call
        UnsupportedEncryption,  // Unsupported encryption algorithm
        BadRevisionID,          // Invalid revision ID syntax
        CorruptRevisionData,    // Revision contains corrupted/unreadable data
        NotOpen,                // Database/KeyStore/index is not open
        NotFound,               // Document not found
        Conflict,               // Document update conflict
        InvalidParameter,       // Invalid function parameter or struct value
        UnexpectedError, /*10*/ // Internal unexpected C++ exception
        CantOpenFile,           // Database file can't be opened; may not exist
        IOError,                // File I/O error
        MemoryError,            // Memory allocation failed (out of memory?)
        NotWriteable,           // File is not writeable
        CorruptData,            // Data is corrupted
        Busy,                   // Database is busy/locked
        NotInTransaction,       // Function must be called while in a transaction
        TransactionNotClosed,   // Database can't be closed while a transaction is open
        Unsupported,            // Operation not supported in this database
        NotADatabaseFile,/*20*/ // File is not a database, or encryption key is wrong
        WrongFormat,            // Database exists but not in the format/storage requested
        Crypto,                 // Encryption/decryption error
        InvalidQuery,           // Invalid query
        MissingIndex,           // No such index, or query requires a nonexistent index
        InvalidQueryParam,      // Unknown query param name, or param number out of range
        RemoteError,            // Unknown error from remote server
        DatabaseTooOld,         // Database file format is older than what I can open
        DatabaseTooNew,         // Database file format is newer than what I can open
        BadDocID,               // Invalid document ID
        CantUpgradeDatabase,/*30*/ // DB can't be upgraded (might be unsupported dev version)
        
        UntranslatableError = 1000,  // Can't translate native error (unknown domain or code)
    }
}

enum_from_primitive! {
    /** Network error codes, in the CBLNetworkDomain. */
    #[derive(Debug, PartialEq)]
    pub enum NetworkError {
        DNSFailure = 1,            // DNS lookup failed
        UnknownHost,               // DNS server doesn't know the hostname
        Timeout,                   // No response received before timeout
        InvalidURL,                // Invalid URL
        TooManyRedirects,          // HTTP redirect loop
        TLSHandshakeFailed,        // Low-level error establishing TLS
        TLSCertExpired,            // Server's TLS certificate has expired
        TLSCertUntrusted,          // Cert isn't trusted for other reason
        TLSClientCertRequired,     // Server requires client to have a TLS certificate
        TLSClientCertRejected,     // Server rejected my TLS client certificate
        TLSCertUnknownRoot,        // Self-signed cert, or unknown anchor cert
        InvalidRedirect,           // Attempted redirect to invalid URL
        Unknown,                   // Unknown networking error
        TLSCertRevoked,            // Server's cert has been revoked
        TLSCertNameMismatch,       // Server cert's name does not match DNS name
    }
}

#[derive(Debug, PartialEq)]
pub enum Error {
    CouchbaseLite   (CouchbaseLiteError),
    POSIX           (i32),
    SQLite          (i32),
    Fleece          (i32),
    Network         (NetworkError),
    WebSocket       (i32)
}


impl Error {
    pub fn new(err: &CBLError) -> Error {
        match err.domain {
            CBLDomain => {
                if let Some(e) = CouchbaseLiteError::from_i32(err.code) {
                    return Error::CouchbaseLite(e)
                }
            }
            CBLNetworkDomain => {
                if let Some(e) = NetworkError::from_i32(err.code) {
                    return Error::Network(e)
                }
            }
            CBLPOSIXDomain => return Error::POSIX(err.code),
            CBLSQLiteDomain => return Error::SQLite(err.code),
            CBLFleeceDomain => return Error::POSIX(err.code),
            CBLWebSocketDomain => return Error::WebSocket(err.code),
            _ => { }
        }
        return Error::untranslatable()
    }
    
    fn untranslatable() -> Error {
        Error::CouchbaseLite(CouchbaseLiteError::UntranslatableError)
    }
}


impl Default for CBLError {
    fn default() -> CBLError { CBLError{domain: 0, code: 0, internal_info: 0} }
}


// Convenient way to return a Result from a failed CBLError
pub fn failure<T>(err: CBLError) -> Result<T, Error> {
    assert!(err.code != 0);
    return Err(Error::new(&err));
}

pub fn check_failure(status: bool, err: &CBLError) -> Result<(), Error> {
    if status {
        return Ok(());
    } else {
        assert!(err.code != 0);
        return Err(Error::new(err));
    }
}

pub fn check_bool<F>(func: F) -> Result<(), Error>
    where F: Fn(*mut CBLError)->bool
{
    let mut error = CBLError::default();
    let ok = func(&mut error);
    return check_failure(ok, &error);
}


//////// SLICES


pub fn as_slice(s: &str) -> FLSlice {
    return FLSlice{buf: s.as_ptr() as *const c_void, 
                   size: s.len() as u64};
}

impl AsRef<[u8]> for FLSlice {
    fn as_ref(&self) -> &[u8] {
        unsafe {
            std::slice::from_raw_parts(self.buf as *const u8, self.size as usize)
        }
    }
}

impl Borrow<str> for FLSlice {
    fn borrow(&self) -> &str {
        unsafe {
            str::from_utf8(std::slice::from_raw_parts(self.buf as *const u8, self.size as usize)).unwrap()
        }
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
