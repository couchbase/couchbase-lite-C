// mod base

use super::*;
use super::base::*;
use super::error::*;
use super::c_api::*;

use enum_primitive::FromPrimitive;
use std::ptr;
use std::str;


pub enum Trust {
    Untrusted,
    Trusted,
}

/// Equivalent to FLDoc
pub struct Fleece {
    _ref: FLDoc,
}

impl Fleece {
    pub fn parse(data: &[u8], trust: Trust) -> Result<Self, Error> {
        unsafe {
            let mut copied = FLSlice_Copy(bytes_as_slice(data));
            let doc = FLDoc_FromResultData(copied, trust as u32, ptr::null_mut(), NULL_SLICE);
            if doc.is_null() {
                copied.release();
                return Err(Error::Fleece(FleeceError::InvalidData));
            }
            return Ok(Fleece{_ref: doc});
        }
    }
    
    pub fn parse_json(json: &str) -> Result<Self, Error> {
        unsafe {
            let mut error: FLError = 0;
            let doc = FLDoc_FromJSON(as_slice(json), &mut error);
            if doc.is_null() {
                return Err(Error::from_fleece(error));
            }
            return Ok(Fleece{_ref: doc});
        }
    }
    
    pub fn root(&self) -> Value {
        unsafe {
            let v = FLDoc_GetRoot(self._ref);
            return Value{_ref: v};
        }
    }
    
    pub fn as_array(&self) -> Array {
        todo!()
    }
    
    pub fn as_dict(&self) -> Dict {
        todo!()
    }
    
    pub fn data<'a>(&self) -> &'a[u8] {
        unsafe {
            return FLDoc_GetData(self._ref).as_byte_array();
        }
    }
}

impl Drop for Fleece {
    fn drop(&mut self) {
        unsafe {
            FLDoc_Release(self._ref);
        }
    }
}

impl Clone for Fleece {
    fn clone(&self) -> Self {
        unsafe {
            return Fleece{_ref: FLDoc_Retain(self._ref)}
        }
    }
}


//////// VALUE


enum_from_primitive! {
    pub enum ValueType {
        Undefined = -1,  // Type of a NULL pointer, i.e. no such value, like JSON `undefined`
        Null = 0,        // Equivalent to a JSON 'null'
        Boolean,         // A `true` or `false` value
        Number,          // A numeric value, either integer or floating-point
        String,          // A string
        Data,            // Binary data (no JSON equivalent)
        Array,           // An array of values
        Dict             // A mapping of strings to values
    }
}


pub struct Value {
    _ref: FLValue
}


impl Value {
    pub fn get_type(&self) -> ValueType {
        unsafe { return ValueType::from_i32(FLValue_GetType(self._ref)).unwrap(); }
    }
    
    pub fn is_integer(&self) -> bool    {unsafe{ return FLValue_IsInteger(self._ref);} }
    pub fn is_unsigned(&self) -> bool   {unsafe{ return FLValue_IsUnsigned(self._ref);} }
    pub fn is_double(&self) -> bool     {unsafe{ return FLValue_IsDouble(self._ref);} }

    pub fn as_integer(&self) -> i64     {unsafe{ return FLValue_AsInt(self._ref);} }
    pub fn as_unsigned(&self) -> u64    {unsafe{ return FLValue_AsUnsigned(self._ref);} }
    pub fn as_double(&self) -> f64      {unsafe{ return FLValue_AsDouble(self._ref);} }
    pub fn as_float(&self) -> f32       {unsafe{ return FLValue_AsFloat(self._ref);} }
    pub fn as_bool(&self) -> bool       {unsafe{ return FLValue_AsBool(self._ref);} }
    
    pub fn as_timestamp(&self) -> Timestamp {
        unsafe { return Timestamp(FLValue_AsTimestamp(self._ref)); } 
    }
    
    pub fn as_string<'a>(&self) -> &'a str {
        unsafe { return FLValue_AsString(self._ref).as_str() };
    }
    
    pub fn as_data<'a>(&self) -> &'a [u8] {
        unsafe { return FLValue_AsString(self._ref).as_byte_array() };
    }
    
    pub fn as_array(&self) -> Array {
        unsafe { return Array{_val: Value{_ref: FLValue_AsArray(self._ref) as FLValue}}; }
    }
    
    pub fn as_dict(&self) -> Dict {
        unsafe { return Dict{_val: Value{_ref: FLValue_AsDict(self._ref) as FLValue}}; }
    }
    
    pub fn to_string(&self) -> String {
        unsafe {
            return FLValue_ToString(self._ref).to_string();
        }
    }
    
    pub fn to_json(&self) -> String {
        unsafe {
            return FLValue_ToJSON(self._ref).to_string();
        }
    }
}


//////// ARRAY


pub struct Array {
    _val: Value
}


//////// DICT


pub struct Dict {
    _val: Value
}
