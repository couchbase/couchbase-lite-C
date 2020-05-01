// mod logging

use super::base::*;
use super::c_api::*;

use enum_primitive::FromPrimitive;
use std::fmt;


enum_from_primitive! {
    #[derive(Debug, Clone, Copy, PartialEq)]
    pub enum Domain {
        All,
        Database,
        Query,
        Replicator,
        Network
    }
}

enum_from_primitive! {
    #[derive(Debug, Clone, Copy, PartialEq)]
    pub enum Level {
        Debug,
        Verbose,
        Info,
        Warning,
        Error,
        None
    }
}


pub type LogCallback = Option<fn(Domain,Level,&str)>;


pub fn set_level(level: Level, domain: Domain) {
    unsafe { CBLLog_SetConsoleLevelOfDomain(domain as u8, level as u8) }
}

pub fn set_callback(callback: LogCallback) {
    unsafe {
        LOG_CALLBACK = callback;
        if callback.is_some() {
            CBLLog_SetCallback(Some(invoke_log_callback));
        } else {
            CBLLog_SetCallback(None);
        }
    }
}

pub fn write(domain: Domain, level: Level, message: &str) {
    unsafe {
        CBL_Log_s(domain as u8, level as u8, as_slice(message));

        // CBL_Log doesn't invoke the callback, so do it manually:
        if let Some(callback) = LOG_CALLBACK {
            if  CBLLog_WillLogToConsole(domain as u8, level as u8) {
                callback(domain, level, message);
            }
        }
    }
}

pub fn write_args(domain: Domain, level: Level, args: fmt::Arguments) {
    write(domain, level, &format!("{:?}", args));
}


//////// LOGGING MACROS:


#[macro_export]
macro_rules! error {
    ($($arg:tt)*) => ($crate::logging::write_args(
        $crate::logging::Domain::All, $crate::logging::Level::Error,
        format_args!($($arg)*)));
}

#[macro_export]
macro_rules! warn {
    ($($arg:tt)*) => ($crate::logging::write_args(
        $crate::logging::Domain::All, $crate::logging::Level::Warning,
        format_args!($($arg)*)));
}

#[macro_export]
macro_rules! info {
    ($($arg:tt)*) => ($crate::logging::write_args(
        $crate::logging::Domain::All, $crate::logging::Level::Info,
        format_args!($($arg)*)));
}

#[macro_export]
macro_rules! verbose {
    ($($arg:tt)*) => ($crate::logging::write_args(
        $crate::logging::Domain::All, $crate::logging::Level::Verbose,
        format_args!($($arg)*)));
}

#[macro_export]
macro_rules! debug {
    ($($arg:tt)*) => ($crate::logging::write_args(
        $crate::logging::Domain::All, $crate::logging::Level::Debug,
        format_args!($($arg)*)));
}


//////// INTERNALS:


static mut LOG_CALLBACK : LogCallback = Some(default_callback);

fn default_callback(_domain: Domain, _level: Level, msg: &str) {
    println!("CBL: {}", msg);
}

unsafe extern "C" fn invoke_log_callback(c_domain: CBLLogDomain, c_level: CBLLogLevel,
                                         msg: *const ::std::os::raw::c_char)
{
    let domain = Domain::from_u8(c_domain).unwrap();
    let level  = Level::from_u8(c_level).unwrap();
    if let Some(cb) = LOG_CALLBACK {
        cb(domain, level, &to_str(msg));
    }
}
