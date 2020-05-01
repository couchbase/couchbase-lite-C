// mod base

use super::*;
use super::base::*;
use super::error::*;
use super::c_api::*;

use enum_primitive::FromPrimitive;
use std::fmt;
use std::marker::PhantomData;
use std::mem::MaybeUninit;
use std::ptr;
use std::str;


//////// CONTAINER


pub enum Trust {
    Untrusted,
    Trusted,
}

/// Equivalent to FLDoc
pub struct Fleece {
    pub(crate) _ref: FLDoc,
}

impl Fleece {
    pub fn parse(data: &[u8], trust: Trust) -> Result<Self> {
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

    pub fn parse_json(json: &str) -> Result<Self> {
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
            Value::wrap(FLDoc_GetRoot(self._ref), self)
        }
    }

    pub fn as_array(&self) -> Array {
        self.root().as_array()
    }

    pub fn as_dict(&self) -> Dict {
        self.root().as_dict()
    }

    pub fn data<'a>(&self) -> &'a[u8] {
        unsafe {
            return FLDoc_GetData(self._ref).as_byte_array().unwrap();
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
    #[derive(Debug, PartialEq)]
    pub enum ValueType {
        Undefined = -1,  // Type of a NULL pointer, i.e. no such value, like JSON `undefined`
        Null = 0,        // Equivalent to a JSON 'null'
        Bool,            // A `true` or `false` value
        Number,          // A numeric value, either integer or floating-point
        String,          // A string
        Data,            // Binary data (no JSON equivalent)
        Array,           // An array of values
        Dict             // A mapping of strings to values
    }
}


/** A Fleece value. It could be any type, including Undefined (empty). */
#[derive(Clone, Copy)]
pub struct Value<'f> {
    pub(crate) _ref: FLValue,
    pub(crate) _owner : PhantomData<&'f Fleece>
}


impl<'f> Value<'f> {
    pub const UNDEFINED : Value<'static> = Value{_ref: ptr::null(), _owner: PhantomData};

    pub(crate) fn wrap<'a, T>(value: FLValue, _owner: &'a T) -> Value<'a> {
        Value{_ref: value, _owner: PhantomData}
    }

    pub fn get_type(&self) -> ValueType {
        unsafe { return ValueType::from_i32(FLValue_GetType(self._ref)).unwrap(); }
    }
    pub fn is_type(&self, t: ValueType) -> bool { self.get_type() == t }

    pub fn is_number(&self)  -> bool    {self.is_type(ValueType::Number)}
    pub fn is_integer(&self) -> bool    {unsafe { FLValue_IsInteger(self._ref) } }

    pub fn as_i64(&self) -> Option<i64>  {if self.is_integer() {Some(self.as_i64_or_0())} else {None} }
    pub fn as_u64(&self) -> Option<u64>  {if self.is_integer() {Some(self.as_u64_or_0())} else {None} }
    pub fn as_f64(&self) -> Option<f64>  {if self.is_number() {Some(self.as_f64_or_0())} else {None} }
    pub fn as_f32(&self) -> Option<f32>  {if self.is_number() {Some(self.as_f32_or_0())} else {None} }
    pub fn as_bool(&self)-> Option<bool> {if self.is_type(ValueType::Bool) {Some(self.as_bool_or_false())} else {None} }

    pub fn as_i64_or_0(&self) -> i64    {unsafe { FLValue_AsInt(self._ref) } }
    pub fn as_u64_or_0(&self) -> u64    {unsafe { FLValue_AsUnsigned(self._ref) } }
    pub fn as_f64_or_0(&self) -> f64    {unsafe { FLValue_AsDouble(self._ref) } }
    pub fn as_f32_or_0(&self) -> f32    {unsafe { FLValue_AsFloat(self._ref) } }
    pub fn as_bool_or_false(&self) -> bool   {unsafe { FLValue_AsBool(self._ref) } }

    pub fn as_timestamp(&self) -> Option<Timestamp> {
        unsafe {
            let t = FLValue_AsTimestamp(self._ref);
            if t == 0 {
                return None;
            }
            return Some(Timestamp(t));
        }
    }

    pub fn as_string(&self) -> Option<&'f str> {
        unsafe { FLValue_AsString(self._ref).as_str() }
    }

    pub fn as_data(&self) -> Option<&'f [u8]> {
        unsafe { FLValue_AsData(self._ref).as_byte_array() }
    }

    pub fn as_array(&self) -> Array<'f> {
        unsafe { Array{_ref: FLValue_AsArray(self._ref), _owner: self._owner} }
    }

    pub fn as_dict(&self) -> Dict<'f> {
        unsafe { Dict{_ref: FLValue_AsDict(self._ref), _owner: self._owner} }
    }

    pub fn to_json(&self) -> String {
        unsafe { FLValue_ToJSON(self._ref).to_string().unwrap() }
    }
}

impl Default for Value<'_> {
    fn default() -> Value<'static> { Value::UNDEFINED }
}

impl PartialEq for Value<'_> {
    fn eq(&self, other: &Self) -> bool {
        unsafe { FLValue_IsEqual(self._ref, other._ref) }
    }
}

impl std::ops::Not for Value<'_> {
    type Output = bool;
    fn not(self) -> bool {self._ref.is_null()}
}

impl Eq for Value<'_> { }

impl fmt::Debug for Value<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Value")
         .field("type", &self.get_type())
         .finish()
    }
}

impl fmt::Display for Value<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        return f.write_str(&self.to_json());
    }
}


//////// ARRAY


/** A Fleece array value. */
#[derive(Clone, Copy)]
pub struct Array<'f> {
    pub(crate) _ref: FLArray,
    pub(crate) _owner : PhantomData<&'f Fleece>
}

impl<'f> Array<'f> {
    pub(crate) fn wrap<'a, T>(array: FLArray, _owner: &'a T) -> Array<'a> {
        Array{_ref: array, _owner: PhantomData}
    }

    pub fn as_value(&self) -> Value { Value::wrap(self._ref as FLValue, self) }

    pub fn count(&self) -> u32 { unsafe { FLArray_Count(self._ref) }}
    pub fn empty(&self) -> bool { unsafe { FLArray_IsEmpty(self._ref) }}

    pub fn get(&self, index: usize) -> Value {
        unsafe { Value::wrap(FLArray_Get(self._ref, index as u32), self) }
    }

    pub fn iter(&self) -> ArrayIterator<'f> {
        unsafe {
            let mut i = MaybeUninit::<FLArrayIterator>::uninit();
            FLArrayIterator_Begin(self._ref, i.as_mut_ptr());
            return ArrayIterator{_innards: i.assume_init(), _owner: self._owner};
        }
    }
}

impl Default for Array<'_> {
    fn default() -> Array<'static> { Array{_ref: ptr::null(), _owner: PhantomData} }
}

impl PartialEq for Array<'_> {
    fn eq(&self, other: &Self) -> bool { self.as_value() == other.as_value() }
}

impl std::ops::Not for Array<'_> {
    type Output = bool;
    fn not(self) -> bool {self._ref.is_null()}
}

impl fmt::Debug for Array<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Array")
         .field("count", &self.count())
         .finish()
    }
}

impl fmt::Display for Array<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        return f.write_str(&self.as_value().to_json());
    }
}

impl<'a> IntoIterator for Array<'a> {
    type Item = Value<'a>;
    type IntoIter = ArrayIterator<'a>;
    fn into_iter(self) -> Self::IntoIter { self.iter() }
}

// This doesn't work because it requires the return value be a ref!
// impl std::ops::Index<usize> for Array {
//     type Output = Value;
//     fn index(&self, index: usize) -> Value { self.get(index) }
// }


//////// ARRAY ITERATOR


pub struct ArrayIterator<'a> {
    _innards : FLArrayIterator,
    _owner : PhantomData<&'a Fleece>
}

impl<'a> ArrayIterator<'a> {
    pub fn count(&self) -> u32 {
        unsafe { FLArrayIterator_GetCount(&self._innards) }
    }

    pub fn get(&self, index: usize) -> Value {
        unsafe {
            Value::wrap(FLArrayIterator_GetValueAt(&self._innards, index as u32), self)
        }
    }
}

impl<'f> Iterator for ArrayIterator<'f> {
    type Item = Value<'f>;

    fn next(&mut self) -> Option<Value<'f>> {
        unsafe {
            let val = FLArrayIterator_GetValue(&self._innards);
            if val.is_null() {
                return None;
            }
            FLArrayIterator_Next(&mut self._innards);
            return Some(Value{_ref: val, _owner: PhantomData});
        }
    }
}
//TODO: Implement FusedIterator, ExactSizeIterator, FromIterator


//////// DICT


/** A Fleece dictionary (object) value. */
#[derive(Clone, Copy)]
pub struct Dict<'f> {
    pub(crate) _ref: FLDict,
    pub(crate) _owner : PhantomData<&'f Fleece>
}

impl<'f> Dict<'f> {
    pub(crate) fn wrap<'a, T>(dict: FLDict, _owner: &'a T) -> Dict<'a> { Dict{_ref: dict, _owner: PhantomData} }

    pub fn as_value(&self) -> Value<'f> { Value{_ref: self._ref as FLValue, _owner: self._owner} }

    pub fn count(&self) -> u32 { unsafe { FLDict_Count(self._ref) }}
    pub fn empty(&self) -> bool { unsafe { FLDict_IsEmpty(self._ref) }}

    pub fn get(&self, key: &str) -> Value<'f> {
        unsafe { Value{_ref: FLDict_Get(self._ref, as_slice(key)), _owner: self._owner} }
    }

    pub fn get_key(&self, key: &mut DictKey) -> Value<'f> {
        unsafe { Value{_ref: FLDict_GetWithKey(self._ref, &mut key._innards), _owner: self._owner} }
    }

    pub fn iter(&self) -> DictIterator<'f> {
        unsafe {
            let mut i = MaybeUninit::<FLDictIterator>::uninit();
            FLDictIterator_Begin(self._ref, i.as_mut_ptr());
            return DictIterator{_innards: i.assume_init(), _owner: self._owner};
        }
    }
}

impl Default for Dict<'_> {
    fn default() -> Dict<'static> { Dict{_ref: ptr::null(), _owner: PhantomData} }
}

impl PartialEq for Dict<'_> {
    fn eq(&self, other: &Self) -> bool { self.as_value() == other.as_value() }
}

impl std::ops::Not for Dict<'_> {
    type Output = bool;
    fn not(self) -> bool {self._ref.is_null()}
}

impl fmt::Debug for Dict<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Dict")
         .field("count", &self.count())
         .finish()
    }
}

impl fmt::Display for Dict<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        return f.write_str(&self.as_value().to_json());
    }
}

impl<'a> IntoIterator for Dict<'a> {
    type Item = (&'a str, Value<'a>);
    type IntoIter = DictIterator<'a>;
    fn into_iter(self) -> Self::IntoIter { self.iter() }
}


//////// DICT KEY


pub struct DictKey {
    pub(crate) _innards: FLDictKey
}

impl DictKey {
    pub fn new(key: &str) -> DictKey {
        unsafe {
            return DictKey{_innards: FLDictKey_Init(as_slice(key))};
        }
    }

    pub fn string(&self) -> &str {
        unsafe { FLDictKey_GetString(&self._innards).as_str().unwrap() }
    }
}


//////// DICT ITERATOR


pub struct DictIterator<'a> {
    _innards : FLDictIterator,
    _owner : PhantomData<&'a Fleece>
}

impl<'a> DictIterator<'a> {
    pub fn count(&self) -> u32 {
        unsafe { FLDictIterator_GetCount(&self._innards) }
    }
}

impl<'a> Iterator for DictIterator<'a> {
    type Item = (&'a str, Value<'a>);

    fn next(&mut self) -> Option<Self::Item> {
        unsafe {
            let val = FLDictIterator_GetValue(&self._innards);
            if val.is_null() {
                return None;
            }
            let key = FLDictIterator_GetKeyString(&self._innards).as_str().unwrap();
            FLDictIterator_Next(&mut self._innards);
            return Some( (key, Value{_ref: val, _owner: PhantomData}) );
        }
    }
}
//TODO: Implement FusedIterator, ExactSizeIterator, FromIterator
