// mod fleece_mutable

use super::*;
use super::base::*;
use super::error::*;
use super::c_api::*;
use super::fleece::*;

use std::fmt;
use std::marker::PhantomData;
use std::ptr;



pub struct MutableDict {
    _ref: FLMutableDict,
}

// Dict API:
impl MutableDict {
    pub fn as_value(&self) -> Value { Value::wrap(self._ref as FLValue, self) }
    pub fn as_dict(&self) -> Dict { Dict::wrap(self._ref, self) }
    pub fn count(&self) -> u32 { self.as_dict().count() }
    pub fn empty(&self) -> bool { self.as_dict().empty() }
    pub fn get(&self, key: &str) -> Value {self.as_dict().get(key)}
    pub fn get_key(&self, key: &mut DictKey) -> Value {self.as_dict().get_key(key)}
    pub fn iter(&self) -> DictIterator {self.as_dict().iter()}
}

impl Clone for MutableDict{
    fn clone(&self) -> Self {
        unsafe{ return MutableDict{_ref: FLValue_Retain(self._ref as FLValue) as FLMutableDict} }
    }
}

impl Drop for MutableDict {
    fn drop(&mut self) {
        unsafe{ FLValue_Release(self._ref as FLValue); }
    }
}

impl Default for MutableDict {
    fn default() -> MutableDict { MutableDict{_ref: ptr::null_mut()} }
}

impl PartialEq for MutableDict {
    fn eq(&self, other: &Self) -> bool { self.as_value() == other.as_value() }
}

impl std::ops::Not for MutableDict {
    type Output = bool;
    fn not(self) -> bool {self._ref.is_null()}
}

impl fmt::Debug for MutableDict {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MutableDict")
         .field("count", &self.count())
         .finish()
    }
}

impl fmt::Display for MutableDict {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        return f.write_str(&self.as_value().to_json());
    }
}

impl<'a> IntoIterator for &'a MutableDict {
    type Item = (&'a str, Value<'a>);
    type IntoIter = DictIterator<'a>;
    fn into_iter(self) -> Self::IntoIter { self.iter() }
}

pub enum CopyFlags {
    Default            = 0,
    Deep               = 1,
    CopyImmutables     = 2,
    DeepCopyImmutables = 3,
}

// Mutable API:
impl MutableDict {
    pub fn new() -> MutableDict {
        unsafe { MutableDict{_ref: FLMutableDict_New()} }
    }

    pub fn from_dict(dict: &Dict) -> MutableDict {
        MutableDict::from_dict_(dict, CopyFlags::Default)
    }

    pub(crate) fn adopt(dict: FLMutableDict) -> MutableDict {
        unsafe {
            FLValue_Retain(dict as FLValue);
            return MutableDict{_ref: dict};
        }
    }

    pub fn from_dict_(dict: &Dict, flags: CopyFlags) -> MutableDict {
        unsafe { MutableDict{_ref: FLDict_MutableCopy(dict._ref, flags as u32)} }
    }

    pub fn is_changed(&self) -> bool {
        unsafe { FLMutableDict_IsChanged(self._ref) }
    }

    pub fn at<'s>(&'s mut self, key: &str) -> Slot<'s> {
        unsafe { Slot{_ref: FLMutableDict_Set(self._ref, as_slice(key)), _owner: PhantomData} }
    }

    pub fn remove(&mut self, key: &str) {
        unsafe { FLMutableDict_Remove(self._ref, as_slice(key)) }
    }

    pub fn remove_all(&mut self) {
        unsafe { FLMutableDict_RemoveAll(self._ref) }
    }
}


//////// SLOT:


pub struct Slot<'s> {
    _ref: FLSlot,
    _owner: PhantomData<&'s mut MutableDict>
}

impl<'s> Slot<'s> {
    pub fn put_null(self)                 { unsafe { FLSlot_SetNull(self._ref) } }
    pub fn put_bool(self, value: bool)    { unsafe { FLSlot_SetBool(self._ref, value) } }
    pub fn put_i64(self, value: i64)      { unsafe { FLSlot_SetInt(self._ref, value) } }
    pub fn put_f64(self, value: f64)      { unsafe { FLSlot_SetDouble(self._ref, value) } }
    pub fn put_string(self, value: &str)  { unsafe { FLSlot_SetString(self._ref, as_slice(value)) } }
    pub fn put_data(self, value: &[u8])   { unsafe { FLSlot_SetString(self._ref, bytes_as_slice(value)) } }
    pub fn put_value(self, value: &Value) { unsafe { FLSlot_SetValue(self._ref, value._ref) } }
}
