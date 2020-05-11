// Fleece mutable-object API bindings, for Couchbase Lite document properties
//
// Copyright (c) 2020 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

use super::*;
use super::slice::*;
use super::c_api::*;
use super::fleece::*;

use std::fmt;
use std::marker::PhantomData;
use std::ptr;



pub enum CopyFlags {
    Default            = 0,     // Shallow copy of mutable values
    Deep               = 1,     // Deep copy of mutable values
    CopyImmutables     = 2,     // Make copies of immutable values too
    DeepCopyImmutables = 3,     // The works
}


//////// MUTABLE ARRAY:


pub struct MutableArray {
    pub(crate) _ref: FLMutableArray,
}

impl MutableArray {
    pub fn new() -> MutableArray {
        unsafe { MutableArray{_ref: FLMutableArray_New()} }
    }

    pub fn from_array(array: &Array) -> MutableArray {
        MutableArray::from_array_(array, CopyFlags::Default)
    }

    pub fn from_array_(array: &Array, flags: CopyFlags) -> MutableArray {
        unsafe { MutableArray{_ref: FLArray_MutableCopy(array._ref, flags as u32)} }
    }

    pub(crate) unsafe fn adopt(array: FLMutableArray) -> MutableArray {
        FLValue_Retain(array as FLValue);
        return MutableArray{_ref: array};
    }

    pub fn is_changed(&self) -> bool {
        unsafe { FLMutableArray_IsChanged(self._ref) }
    }

    pub fn at<'s>(&'s mut self, index: u32) -> Slot<'s> {
        unsafe { Slot{_ref: FLMutableArray_Set(self._ref, index), _owner: PhantomData} }
    }

    pub fn append<'s>(&'s mut self) -> Slot<'s> {
        unsafe { Slot{_ref: FLMutableArray_Append(self._ref), _owner: PhantomData} }
    }

    pub fn insert<'s>(&'s mut self, index: u32) {
        unsafe { FLMutableArray_Insert(self._ref, index, 1) }
    }

    pub fn remove(&mut self, index: u32) {
        unsafe { FLMutableArray_Remove(self._ref, index, 1) }
    }

    pub fn remove_all(&mut self) {
        unsafe { FLMutableArray_Remove(self._ref, 0, self.count()) }
    }
}

// "Inherited" API:
impl MutableArray {
    pub fn as_array(&self) -> Array          { Array::wrap(self._ref, self) }
    pub fn count(&self) -> u32               { self.as_array().count() }
    pub fn empty(&self) -> bool              { self.as_array().empty() }
    pub fn get(&self, index: u32) -> Value   { self.as_array().get(index) }
    pub fn iter(&self) -> ArrayIterator      { self.as_array().iter() }
}

impl FleeceReference for MutableArray {
    fn _fleece_ref(&self) -> FLValue { self._ref as FLValue }
}

impl Clone for MutableArray{
    fn clone(&self) -> Self {
        unsafe{ return MutableArray{_ref: FLValue_Retain(self._ref as FLValue) as FLMutableArray} }
    }
}

impl Drop for MutableArray {
    fn drop(&mut self) {
        unsafe{ FLValue_Release(self._ref as FLValue); }
    }
}

impl Default for MutableArray {
    fn default() -> MutableArray { MutableArray{_ref: ptr::null_mut()} }
}

impl PartialEq for MutableArray {
    fn eq(&self, other: &Self) -> bool { self.as_value() == other.as_value() }
}

impl Eq for MutableArray { }

impl std::ops::Not for MutableArray {
    type Output = bool;
    fn not(self) -> bool {self._ref.is_null()}
}

impl fmt::Debug for MutableArray {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("MutableArray")
         .field("count", &self.count())
         .finish()
    }
}

impl fmt::Display for MutableArray {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        return f.write_str(&self.as_value().to_json());
    }
}

impl<'a> IntoIterator for &'a MutableArray {
    type Item = Value<'a>;
    type IntoIter = ArrayIterator<'a>;
    fn into_iter(self) -> Self::IntoIter { self.iter() }
}


// Mutable API additions for Array:
impl<'d> Array<'d> {
    pub fn as_mutable(self) -> Option<MutableArray> {
        unsafe {
            let md = FLArray_AsMutable(self._ref);
            return if md.is_null() { None } else { Some(MutableArray::adopt(md)) };
        }
    }

    pub fn mutable_copy(&self) -> MutableArray {
        MutableArray::from_array(self)
    }
}


//////// MUTABLE DICT:


pub struct MutableDict {
    pub(crate) _ref: FLMutableDict,
}

impl MutableDict {
    pub fn new() -> MutableDict {
        unsafe { MutableDict{_ref: FLMutableDict_New()} }
    }

    pub fn from_dict(dict: &Dict) -> MutableDict {
        MutableDict::from_dict_(dict, CopyFlags::Default)
    }

    pub fn from_dict_(dict: &Dict, flags: CopyFlags) -> MutableDict {
        unsafe { MutableDict{_ref: FLDict_MutableCopy(dict._ref, flags as u32)} }
    }

    pub(crate) unsafe fn adopt(dict: FLMutableDict) -> MutableDict {
        FLValue_Retain(dict as FLValue);
        return MutableDict{_ref: dict};
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

// "Inherited" API:
impl MutableDict {
    pub fn as_dict(&self) -> Dict                       { Dict::wrap(self._ref, self) }
    pub fn count(&self) -> u32                          { self.as_dict().count() }
    pub fn empty(&self) -> bool                         { self.as_dict().empty() }
    pub fn get(&self, key: &str) -> Value               { self.as_dict().get(key) }
    pub fn get_key(&self, key: &mut DictKey) -> Value   { self.as_dict().get_key(key) }
    pub fn iter(&self) -> DictIterator                  { self.as_dict().iter() }
}

impl FleeceReference for MutableDict {
    fn _fleece_ref(&self) -> FLValue { self._ref as FLValue }
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

impl Eq for MutableDict { }

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


// Mutable API for Dict:
impl<'d> Dict<'d> {
    pub fn as_mutable(self) -> Option<MutableDict> {
        unsafe {
            let md = FLDict_AsMutable(self._ref);
            return if md.is_null() { None } else { Some(MutableDict::adopt(md)) };
        }
    }

    pub fn mutable_copy(&self) -> MutableDict {
        MutableDict::from_dict(self)
    }
}


//////// SLOT:


/** A reference to an element of a MutableArray or MutableDict,
    for the sole purpose of storing a value in it. */
pub struct Slot<'s> {
    pub(crate) _ref: FLSlot,
    _owner: PhantomData<&'s mut MutableDict>
}

impl<'s> Slot<'s> {
    pub fn put_null(self)                 { unsafe { FLSlot_SetNull(self._ref) } }

    pub fn put_bool(self, value: bool)    { unsafe { FLSlot_SetBool(self._ref, value) } }

    pub fn put_i64<INT: Into<i64>>(self, value: INT) {
        unsafe { FLSlot_SetInt(self._ref, value.into()) }
    }

    pub fn put_f64<F: Into<f64>>(self, value: F) {
        unsafe { FLSlot_SetDouble(self._ref, value.into()) }
    }

    pub fn put_string<STR: AsRef<str>>(self, value: STR) {
        unsafe { FLSlot_SetString(self._ref, as_slice(value.as_ref())) }
    }

    pub fn put_data<DATA: AsRef<[u8]>>(self, value: DATA) {
        unsafe { FLSlot_SetString(self._ref, bytes_as_slice(value.as_ref())) }
    }

    pub fn put_value<VALUE: FleeceReference>(self, value: &VALUE)  {
        unsafe { FLSlot_SetValue(self._ref, value._fleece_ref()) }
    }
}
