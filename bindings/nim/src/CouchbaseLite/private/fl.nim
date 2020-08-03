## *** NOTE: DO NOT MACHINE-UPDATE THIS FILE ***
##
## This started out as a machine-generated file (produced by `gen-bindings.sh`),
## but it has been hand-edited to fix issues that kept it from compiling.
##
## If you run the script again, it will generate a _new_ file `Fleece-new.nim`.
## You'll need to merge new/changed declarations from that file into this one, by hand.
##
## Status: Up-to-date as of 5 May 2020, commit 036cd9f7 "Export kCBLAuthDefaultCookieName...",
##         Fleece commit 4ca3dbf1, "Mutable.hh: Allow conversion of keyRef and FLSlot to FLSlot".


when defined(Linux):
  {.push dynlib: "libCouchbaseLiteC.so".}
elif defined(MacOS) or defined(MacOSX):
  {.push dynlib: "libCouchbaseLiteC.dylib".}
elif defined(Windows):
  {.push dynlib: "CouchbaseLiteC.dll".}


##   FLSlice.h
##   Fleece
##
##   Created by Jens Alfke on 8/13/18.
##   Copyright © 2018 Couchbase. All rights reserved.
##
## A simple reference to a block of memory. Does not imply ownership.
##     (This is equivalent to the C++ class `slice`.)
type
  FLSlice* {.bycopy.} = object
    buf*: pointer
    size*: csize_t


## A block of memory returned from an API call. The caller takes ownership, and must call
##     FLSlice_Release (or FLSlice_Free) when done. The heap block may be shared with other users,
##     so it must not be modified.
##     (This is equivalent to the C++ class `alloc_slice`.)
type
  FLSliceResult* {.bycopy.} = object
    buf*: pointer
    size*: csize_t


## A heap-allocated, reference-counted slice. This type is really just a hint in an API
##     that the data can be retained instead of copied, by assigning it to an alloc_slice.
##     You can just treat it like FLSlice.
type
  FLHeapSlice* = FLSlice

type
  FLString* = FLSlice
  FLStringResult* = FLSliceResult

## Slice <-> string conversion:
proc flStr*(str: string): FLSlice =
  FLSlice(buf: cstring(str), size: csize_t(len(str)))


## Equality test of two slices.
proc equal*(a: FLSlice; b: FLSlice): bool {.importc: "FLSlice_Equal".}

## Lexicographic comparison of two slices; basically like memcmp(), but taking into account
##     differences in length.
proc compare*(a1: FLSlice; a2: FLSlice): cint {.importc: "FLSlice_Compare".}

## Allocates an FLSliceResult of the given size, without initializing the buffer.
proc newSliceResult*(a1: csize_t): FLSliceResult {.importc: "FLSliceResult_New".}

## Allocates an FLSliceResult, copying the given slice.
proc copy*(a1: FLSlice): FLSliceResult {.importc: "FLSlice_Copy".}

proc internalFLBufRetain(a1: pointer) {.importc: "_FLBuf_Retain".}
proc internalFLBufRelease(a1: pointer) {.importc: "_FLBuf_Release".}


# Hand-written: Automatic ref-counting for FLSliceResult
# (see: <https://nim-lang.org/docs/destructors.html>)

proc `=destroy`(s: var FLSliceResult) =
  internalFLBufRelease(s.buf)

proc `=`(dest: var FLSliceResult; source: FLSliceResult) =
  let oldBuf = dest.buf
  if oldBuf != source.buf:
    internalFLBufRetain(source.buf)
    dest.buf = source.buf
    internalFLBufRelease(oldBuf)
  dest.size = source.size


#%%%%%%% NIM CONVERSIONS FOR SLICES:
proc asSlice*(s: FLSliceResult): fl.FLSlice =
  FLSlice(buf: s.buf, size: s.size)

proc asSlice*(bytes: openarray[byte]): fl.FLSlice =
  FLSlice(buf: unsafeAddr bytes[0], size: csize_t(bytes.len))

proc asSlice*(str: string): fl.FLSlice =
  FLSlice(buf: unsafeAddr str[0], size: csize_t(str.len))

proc toString*(s: FLSlice): string =
  if s.buf == nil: return ""
  var str = newString(s.size)
  copyMem(addr str[0], s.buf, s.size)
  return str

proc toString*(s: FLSliceResult): string =
  toString(s.asSlice())

proc toByteArray*(s: FLSlice): seq[uint8] =
  var bytes = newSeq[uint8](s.size)
  copyMem(addr bytes[0], s.buf, s.size)
  return bytes

##
##  Fleece.h
##
##  Copyright (c) 2016 Couchbase, Inc All rights reserved.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##  http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
##  This is the C API! For the C++ API, see Fleece.hh.

## ////// BASIC TYPES
type
  FLValue* = ptr object ## A reference to a value of any type.
  FLArray* = ptr object ## A reference to an array value.
  FLDict* = ptr object ## A reference to a dictionary (map) value.
  FLSlot* = ptr object ## A reference to a mutable array/dict item
  FLMutableArray* = ptr object ## A reference to a mutable array.
  FLMutableDict* = ptr object ## A reference to a mutable dictionary.

## Error codes returned from some API calls.
type
  FLError* {.size: sizeof(cint).} = enum
    NoError = 0,
    MemoryError,             ##  Out of memory, or allocation failed
    OutOfRange,              ##  Array index or iterator out of range
    InvalidData,             ##  Bad input data (NaN, non-string key, etc.)
    EncodeError,             ##  Structural error encoding (missing value, too many ends, etc.)
    JSONError,               ##  Error parsing JSON
    UnknownValue,            ##  Unparseable data in a FLValue (corrupt? Or from some distant future?)
    InternalError,           ##  Something that shouldn't happen
    NotFound,                ##  Key not found
    SharedKeysStateError,    ##  Misuse of shared keys (not in transaction, etc.)
    POSIXError,              ##  POSIX API call failed
    Unsupported              ##  Operation is unsupported



## ////// DOCUMENT
##             An FLDoc points to (and often owns) Fleece-encoded data and provides access to its
##             Fleece values.
##
type
  FLDoc* = ptr object ## A reference to a document.
  SharedKeys* = ptr object ## A reference to a shared-keys mapping.

## Specifies whether not input data is trusted to be 100% valid Fleece.
type                          ## Input data is not trusted to be valid, and will be fully validated by the API call.
  FLTrust* {.size: sizeof(cint).} = enum
    kUntrusted,               ## Input data is trusted to be valid. The API will perform only minimal validation when
               ##             reading it. This is faster than kFLUntrusted, but should only be used if
               ##             the data was generated by a trusted encoder and has not been altered or corrupted. For
               ##             example, this can be used to parse Fleece data previously stored by your code in local
               ##             storage.
               ##             If invalid data is read by this call, subsequent calls to FLValue accessor functions can
               ##             crash or return bogus results (including data from arbitrary memory locations.)
    kTrusted


## Creates an FLDoc from Fleece-encoded data that's been returned as a result from
##         FLSlice_Copy or other API. The resulting document retains the data, so you don't need to
##         worry about it remaining valid.
proc newDocFromResultData*(data: FLSliceResult; a2: FLTrust; a3: SharedKeys; externData: FLSlice): FLDoc {.importc: "FLDoc_FromResultData".}

## Creates an FLDoc from JSON-encoded data. The data is first encoded into Fleece, and the
##         Fleece data is kept by the doc; the input JSON data is no longer needed after this
##         function returns.
proc newDocFromJSON*(json: FLSlice; outError: var FLError): FLDoc {.importc: "FLDoc_FromJSON".}

## Releases a reference to an FLDoc. This must be called once to free an FLDoc you created.
proc release*(a1: FLDoc) {.importc: "FLDoc_Release".}

## Adds a reference to an FLDoc. This extends its lifespan until at least such time as you
##         call FLRelease to remove the reference.
proc retain*(a1: FLDoc): FLDoc {.importc: "FLDoc_Retain".}

## Returns the encoded Fleece data backing the document.
proc getData*(a1: FLDoc): FLSlice {.importc: "FLDoc_GetData".}

## Returns the FLSliceResult data owned by the document, if any, else a null slice.
proc getAllocedData*(a1: FLDoc): FLSliceResult {.importc: "FLDoc_GetAllocedData".}

## Returns the root value in the FLDoc, usually an FLDict.
proc getRoot*(a1: FLDoc): FLValue {.importc: "FLDoc_GetRoot".}

## Returns the FLSharedKeys used by this FLDoc, as specified when it was created.
proc getSharedKeys*(a1: FLDoc): SharedKeys {.importc: "FLDoc_GetSharedKeys".}

## Looks up the FLDoc containing the FLValue, or NULL if the FLValue was created without a FLDoc.
##         Caller must release the FLDoc reference!!
proc findDoc*(a1: FLValue): FLDoc {.importc: "FLValue_FindDoc".}



## Returns a pointer to the root value in the encoded data, or NULL if validation failed.
##         The FLValue, and all values found through it, are only valid as long as the encoded data
##         remains intact and unchanged.
proc newValueFromData*(data: FLSlice; a2: FLTrust): FLValue {.importc: "FLValue_FromData".}

## Directly converts JSON data to Fleece-encoded data.
##         You can then call FLValue_FromData (in kFLTrusted mode) to get the root as a FLValue.
proc convertJSON*(json: FLSlice; outError: var FLError): FLSliceResult {.importc: "FLData_ConvertJSON".}

## Produces a human-readable dump of the FLValue encoded in the data.
##         This is only useful if you already know, or want to learn, the encoding format.
proc dump*(data: FLSlice): FLStringResult {.importc: "FLData_Dump".}

##         These are convenience functions that directly return JSON-encoded output.
##         For more control over the encoding, use an FLEncoder.
## Encodes a Fleece value as JSON (or a JSON fragment.)
##         Any Data values will become base64-encoded JSON strings.
proc toJSON*(a1: FLValue): FLStringResult {.importc: "FLValue_ToJSON".}

## Encodes a Fleece value as JSON5, a more lenient variant of JSON that allows dictionary
##         keys to be unquoted if they're alphanumeric. This tends to be more readable.
proc toJSON5*(v: FLValue): FLStringResult {.importc: "FLValue_ToJSON5".}

## Most general Fleece to JSON converter.
proc toJSONX*(v: FLValue; json5: bool; canonicalForm: bool): FLStringResult {.importc: "FLValue_ToJSONX".}

## Converts valid JSON5 <https://json5.org> to JSON. Among other things, it converts single
##         quotes to double, adds missing quotes around dictionary keys, removes trailing commas,
##         and removes comments.
##               comparably invalid JSON, in which case the caller's subsequent JSON parsing will
##               detect the error. The types of errors it overlooks tend to be subtleties of string
##               or number encoding.
##                         As this is a \ref FLStringResult, you will be responsible for freeing it.
##                         will be stored here (if it's not NULL.)
proc jSON5ToJSON*(json5: FLString; outErrorMessage: ptr FLStringResult; outErrorPos: ptr csize_t; outError: var FLError): FLStringResult {.importc: "FLJSON5_ToJSON".}

## Debugging function that returns a C string of JSON. Does not free the string's memory!
proc dump*(a1: FLValue): cstring {.importc: "FLDump".}

## Debugging function that returns a C string of JSON. Does not free the string's memory!
proc dumpData*(data: FLSlice): cstring {.importc: "FLDumpData".}



## ////// VALUE
##         The core Fleece data type is FLValue: a reference to a value in Fleece-encoded data.
##         An FLValue can represent any JSON type (plus binary data).
##
##         - Scalar data types -- numbers, booleans, null, strings, data -- can be accessed
##           using individual functions of the form `FLValue_As...`; these return the scalar value,
##           or a default zero/false/null value if the value is not of that type.
##         - Collections -- arrays and dictionaries -- have their own "subclasses": FLArray and
##           FLDict. These have the same pointer values as an FLValue but are not type-compatible
##           in C. To coerce an FLValue to a collection type, call FLValue_AsArray or FLValue_AsDict.
##           If the value is not of that type, NULL is returned. (FLArray and FLDict are documented
##           fully in their own sections.)
##
##         It's always safe to pass a NULL value to an accessor; that goes for FLDict and FLArray
##         as well as FLValue. The result will be a default value of that type, e.g. false or 0
##         or NULL, unless otherwise specified.

## Types of Fleece values. Basically JSON, with the addition of Data (raw blob).
type
  FLValueType* {.size: sizeof(cint).} = enum
    kUndefined = -1,          ## Type of a NULL pointer, i.e. no such value, like JSON `undefined`. Also the type of a value created by FLEncoder_WriteUndefined().
    kNull = 0,                ## Equivalent to a JSON 'null'
    kBoolean,                 ## A `true` or `false` value
    kNumber,                  ## A numeric value, either integer or floating-point
    kString,                  ## A string
    kData,                    ## Binary data (no JSON equivalent)
    kArray,                   ## An array of values
    kDict                     ## A mapping of strings to values


## A timestamp, expressed as milliseconds since the Unix epoch (1-1-1970 midnight UTC.)
type
  FLTimestamp* = distinct int64

## A value representing a missing timestamp; returned when a date cannot be parsed.
const
  TimestampNone* = FLTimestamp(-0x7FFFFFFFFFFFFFFF'i64)

## Returns the data type of an arbitrary FLValue.
##         (If the parameter is a NULL pointer, returns `kFLUndefined`.)
proc getType*(a1: FLValue): FLValueType {.importc: "FLValue_GetType".}

## Returns true if the value is non-NULL and represents an integer.
proc isInteger*(a1: FLValue): bool {.importc: "FLValue_IsInteger".}

## Returns true if the value is non-NULL and represents an integer >= 2^63. Such a value can't
##         be represented in C as an `int64_t`, only a `uint64_t`, so you should access it by calling
##         `FLValueAsUnsigned`, _not_ FLValueAsInt, which would return  an incorrect (negative)
##         value.
proc isUnsigned*(a1: FLValue): bool {.importc: "FLValue_IsUnsigned".}

## Returns true if the value is non-NULL and represents a 64-bit floating-point number.
proc isDouble*(a1: FLValue): bool {.importc: "FLValue_IsDouble".}

## Returns a value coerced to boolean. This will be true unless the value is NULL (undefined),
##         null, false, or zero.
proc asBool*(a1: FLValue): bool {.importc: "FLValue_AsBool".}

## Returns a value coerced to an integer. True and false are returned as 1 and 0, and
##         floating-point numbers are rounded. All other types are returned as 0.
##         check for these by calling `FLValueIsUnsigned`.
proc asInt*(a1: FLValue): int64 {.importc: "FLValue_AsInt".}

## Returns a value coerced to an unsigned integer.
##         This is the same as `FLValueAsInt` except that it _can't_ handle negative numbers, but
##         does correctly return large `uint64_t` values of 2^63 and up.
proc asUnsigned*(a1: FLValue): uint64 {.importc: "FLValue_AsUnsigned".}

## Returns a value coerced to a 32-bit floating point number.
##         True and false are returned as 1.0 and 0.0, and integers are converted to float. All other
##         types are returned as 0.0.
##         limitations of IEEE 32-bit float format.
proc asFloat*(a1: FLValue): cfloat {.importc: "FLValue_AsFloat".}

## Returns a value coerced to a 32-bit floating point number.
##         True and false are returned as 1.0 and 0.0, and integers are converted to float. All other
##         types are returned as 0.0.
##         the limitations of IEEE 32-bit float format.
proc asDouble*(a1: FLValue): cdouble {.importc: "FLValue_AsDouble".}

## Returns the exact contents of a string value, or null for all other types.
proc asString*(a1: FLValue): FLString {.importc: "FLValue_AsString".}

## Converts a value to a timestamp, in milliseconds since Unix epoch, or INT64_MIN on failure.
##         - A string is parsed as ISO-8601 (standard JSON date format).
##         - A number is interpreted as a timestamp and returned as-is.
proc asTimestamp*(a1: FLValue): FLTimestamp {.importc: "FLValue_AsTimestamp".}

## Returns the exact contents of a data value, or null for all other types.
proc asData*(a1: FLValue): FLSlice {.importc: "FLValue_AsData".}

## If a FLValue represents an array, returns it cast to FLArray, else NULL.
proc asArray*(a1: FLValue): FLArray {.importc: "FLValue_AsArray".}

## If a FLValue represents a dictionary, returns it as an FLDict, else NULL.
proc asDict*(a1: FLValue): FLDict {.importc: "FLValue_AsDict".}

## Returns a string representation of any scalar value. Data values are returned in raw form.
##         Arrays and dictionaries don't have a representation and will return NULL.
proc toString*(a1: FLValue): FLStringResult {.importc: "FLValue_ToString".}

## Compares two values for equality. This is a deep recursive comparison.
proc isEqual*(v1: FLValue; v2: FLValue): bool {.importc: "FLValue_IsEqual".}

## If this value is mutable (and thus heap-based) its ref-count is incremented.
##         Otherwise, this call does nothing.
proc retain*(a1: FLValue): FLValue {.importc: "FLValue_Retain".}

## If this value is mutable (and thus heap-based) its ref-count is decremented, and if it
##         reaches zero the value is freed.
##         If the value is not mutable, this call does nothing.
proc release*(a1: FLValue) {.importc: "FLValue_Release".}
proc retain*(v: FLArray): FLArray {.inline.} =
  return cast[FLArray](retain(cast[FLValue](v)))

proc release*(v: FLArray) {.inline.} =
  release(cast[FLValue](v))

proc retain*(v: FLDict): FLDict {.inline.} =
  return cast[FLDict](retain(cast[FLValue](v)))

proc release*(v: FLDict) {.inline.} =
  release(cast[FLValue](v))

var kNullValue* {.importc: "kFLNullValue".}: FLValue



## ////// VALUE SLOT
proc setNull*(a1: FLSlot) {.importc: "FLSlot_SetNull".} ## Stores a JSON null into a slot.

proc set*(a1: FLSlot; a2: bool) {.importc: "FLSlot_SetBool".} ## Stores a boolean into a slot.

proc set*(a1: FLSlot; a2: int64) {.importc: "FLSlot_SetInt".} ## Stores an integer into a slot.

proc set*(a1: FLSlot; a2: uint64) {.importc: "FLSlot_SetUInt".} ## Stores an unsigned integer into a slot.

proc set*(a1: FLSlot; a2: cfloat) {.importc: "FLSlot_SetFloat".} ## Stores a float into a slot.

proc set*(a1: FLSlot; a2: cdouble) {.importc: "FLSlot_SetDouble".} ## Stores a double into a slot.

proc set*(a1: FLSlot; a2: FLString) {.importc: "FLSlot_SetString".} ## Stores a string into a slot.

proc setData*(a1: FLSlot; a2: FLSlice) {.importc: "FLSlot_SetData".} ## Stores a data blob into a slot.

proc set*(a1: FLSlot; a2: FLValue) {.importc: "FLSlot_SetValue".} ## Stores an FLValue into a slot.



## ////// ARRAY
##         FLArray is a "subclass" of FLValue, representing values that are arrays. It's always OK to
##         pass an FLArray to a function parameter expecting an FLValue, even though the compiler
##         makes you use an explicit type-cast. It's safe to type-cast the other direction, from
##         FLValue to FLArray, _only_ if you already know that the value is an array, e.g. by having
##         called FLValue_GetType on it. But it's safer to call FLValue_AsArray instead, since it
##         will return NULL if the value isn't an array.

## Returns the number of items in an array, or 0 if the pointer is NULL.
proc count*(a1: FLArray): uint32 {.importc: "FLArray_Count".}

## Returns true if an array is empty (or NULL). Depending on the array's representation,
##         this can be faster than `FLArray_Count(a) == 0`
proc isEmpty*(a1: FLArray): bool {.importc: "FLArray_IsEmpty".}

## If the array is mutable, returns it cast to FLMutableArray, else NULL.
proc asMutable*(a1: FLArray): FLMutableArray {.importc: "FLArray_AsMutable".}

## Returns an value at an array index, or NULL if the index is out of range.
proc get*(a1: FLArray; index: uint32): FLValue {.importc: "FLArray_Get".}
var kEmptyArray* {.importc: "kFLEmptyArray".}: FLArray

## Iterating an array typically looks like this:
##
## ```
## FLArrayIterator iter;
## FLArrayIterator_Begin(theArray, &iter);
## FLValue value;
## while (NULL != (value = FLArrayIterator_GetValue(&iter))) {
##   // ...
##   FLArrayIterator_Next(&iter);
## }
## ```


## Opaque array iterator. Declare one on the stack and pass its address to
##         `FLArrayIteratorBegin`.
type
  FLArrayIterator* {.bycopy.} = object
    internalprivate1*: pointer
    internalprivate2*: uint32
    internalprivate3*: bool
    internalprivate4*: pointer

## Initializes a FLArrayIterator struct to iterate over an array.
##         Call FLArrayIteratorGetValue to get the first item, then FLArrayIteratorNext.
proc begin*(a1: FLArray; a2: ptr FLArrayIterator) {.importc: "FLArrayIterator_Begin".}

## Returns the current value being iterated over.
proc getValue*(a1: ptr FLArrayIterator): FLValue {.importc: "FLArrayIterator_GetValue".}

## Returns a value in the array at the given offset from the current value.
proc getValueAt*(a1: ptr FLArrayIterator; offset: uint32): FLValue {.importc: "FLArrayIterator_GetValueAt".}

## Returns the number of items remaining to be iterated, including the current one.
proc getCount*(a1: ptr FLArrayIterator): uint32 {.importc: "FLArrayIterator_GetCount".}

## Advances the iterator to the next value, or returns false if at the end.
proc next*(a1: ptr FLArrayIterator): bool {.importc: "FLArrayIterator_Next".}



## ////// MUTABLE ARRAY


type
  CopyFlags* {.size: sizeof(cint).} = enum
    DefaultCopy = 0, DeepCopy = 1, CopyImmutables = 2, DeepCopyImmutables = 3


## Creates a new mutable FLArray that's a copy of the source FLArray.
##         Its initial ref-count is 1, so a call to FLMutableArray_Release will free it.
##
##         Copying an immutable FLArray is very cheap (only one small allocation) unless the flag
##         kFLCopyImmutables is set.
##
##         Copying a mutable FLArray is cheap if it's a shallow copy, but if `deepCopy` is true,
##         nested mutable Arrays and Dicts are also copied, recursively; if kFLCopyImmutables is
##         also set, immutable values are also copied.
##
##         If the source FLArray is NULL, then NULL is returned.
proc mutableCopy*(a1: FLArray; a2: CopyFlags): FLMutableArray {.importc: "FLArray_MutableCopy".}

## Creates a new empty mutable FLArray.
##         Its initial ref-count is 1, so a call to FLMutableArray_Free will free it.
proc newMutableArray*(): FLMutableArray {.importc: "FLMutableArray_New".}

## Increments the ref-count of a mutable FLArray.
proc retain*(d: FLMutableArray): FLMutableArray {.inline.} =
  return cast[FLMutableArray](retain(cast[FLValue](d)))

## Decrements the refcount of (and possibly frees) a mutable FLArray.
proc release*(d: FLMutableArray) {.inline.} =
  release(cast[FLValue](d))

## If the FLArray was created by FLArray_MutableCopy, returns the original source FLArray.
proc getSource*(a1: FLMutableArray): FLArray {.importc: "FLMutableArray_GetSource".}

## Returns true if the FLArray has been changed from the source it was copied from.
proc isChanged*(a1: FLMutableArray): bool {.importc: "FLMutableArray_IsChanged".}

## Lets you store a value into a FLMutableArray, by returning a \ref FLSlot that you can call
##         a function like \ref FLSlot_SetInt on.
proc set*(a1: FLMutableArray; index: uint32): FLSlot {.importc: "FLMutableArray_Set".}

## Appends a null value to a FLMutableArray and returns a \ref FLSlot that you can call
##         to store something else in the new value.
proc append*(a1: FLMutableArray): FLSlot {.importc: "FLMutableArray_Append".}

## Inserts a contiguous range of JSON `null` values into the array.
proc insert*(array: FLMutableArray; firstIndex: uint32; count: uint32) {.importc: "FLMutableArray_Insert".}

## Removes contiguous items from the array.
proc remove*(array: FLMutableArray; firstIndex: uint32; count: uint32) {.importc: "FLMutableArray_Remove".}

## Changes the size of an array.
##         If the new size is larger, the array is padded with JSON `null` values.
##         If it's smaller, values are removed from the end.
proc resize*(array: FLMutableArray; size: uint32) {.importc: "FLMutableArray_Resize".}

## Convenience function for getting an array-valued property in mutable form.
##         - If the value for the key is not an array, returns NULL.
##         - If the value is a mutable array, returns it.
##         - If the value is an immutable array, this function makes a mutable copy, assigns the
##           copy as the property value, and returns the copy.
proc getMutableArray*(a1: FLMutableArray; index: uint32): FLMutableArray {.importc: "FLMutableArray_GetMutableArray".}

## Convenience function for getting an array-valued property in mutable form.
##         - If the value for the key is not an array, returns NULL.
##         - If the value is a mutable array, returns it.
##         - If the value is an immutable array, this function makes a mutable copy, assigns the
##           copy as the property value, and returns the copy.
proc getMutableDict*(a1: FLMutableArray; index: uint32): FLMutableDict {.importc: "FLMutableArray_GetMutableDict".}



## ////// DICT

## Returns the number of items in a dictionary, or 0 if the pointer is NULL.
proc count*(a1: FLDict): uint32 {.importc: "FLDict_Count".}

## Returns true if a dictionary is empty (or NULL). Depending on the dictionary's
##         representation, this can be faster than `FLDict_Count(a) == 0`
proc isEmpty*(a1: FLDict): bool {.importc: "FLDict_IsEmpty".}

## If the dictionary is mutable, returns it cast to FLMutableDict, else NULL.
proc asMutable*(a1: FLDict): FLMutableDict {.importc: "FLDict_AsMutable".}

## Looks up a key in a dictionary, returning its value.
##         Returns NULL if the value is not found or if the dictionary is NULL.
proc get*(a1: FLDict; keyString: FLSlice): FLValue {.importc: "FLDict_Get".}
var kEmptyDict* {.importc: "kFLEmptyDict".}: FLDict

## Iterating a dictionary typically looks like this:
##
## ```
## FLDictIterator iter;
## FLDictIterator_Begin(theDict, &iter);
## FLValue value;
## while (NULL != (value = FLDictIterator_GetValue(&iter))) {
##     FLString key = FLDictIterator_GetKeyString(&iter);
##     // ...
##     FLDictIterator_Next(&iter);
## }
## ```
##
## Opaque dictionary iterator. Declare one on the stack, and pass its address to
##         FLDictIterator_Begin.
type
  FLDictIterator* {.bycopy.} = object
    internalprivate1*: pointer
    internalprivate2*: uint32
    internalprivate3*: bool
    internalprivate4*: array[4, pointer]
    internalprivate5*: cint

## Initializes a FLDictIterator struct to iterate over a dictionary.
##         Call FLDictIterator_GetKey and FLDictIterator_GetValue to get the first item,
##         then FLDictIterator_Next.
proc begin*(a1: FLDict; a2: ptr FLDictIterator) {.importc: "FLDictIterator_Begin".}

## Returns the current key being iterated over. This FLValue will be a string or an integer.
proc getKey*(a1: ptr FLDictIterator): FLValue {.importc: "FLDictIterator_GetKey".}

## Returns the current key's string value.
proc getKeyString*(a1: ptr FLDictIterator): FLString {.importc: "FLDictIterator_GetKeyString".}

## Returns the current value being iterated over.
proc getValue*(a1: ptr FLDictIterator): FLValue {.importc: "FLDictIterator_GetValue".}

## Returns the number of items remaining to be iterated, including the current one.
proc getCount*(a1: ptr FLDictIterator): uint32 {.importc: "FLDictIterator_GetCount".}

## Advances the iterator to the next value, or returns false if at the end.
proc next*(a1: ptr FLDictIterator): bool {.importc: "FLDictIterator_Next".}

## Cleans up after an iterator. Only needed if (a) the dictionary is a delta, and
##         (b) you stop iterating before the end (i.e. before FLDictIterator_Next returns false.)
proc `end`*(a1: ptr FLDictIterator) {.importc: "FLDictIterator_End".}

## Opaque key for a dictionary. You are responsible for creating space for these; they can
##         go on the stack, on the heap, inside other objects, anywhere.
##         Be aware that the lookup operations that use these will write into the struct to store
##         "hints" that speed up future searches.
type
  FLDictKey* {.bycopy.} = object
    internalprivate1*: FLSlice
    internalprivate2*: pointer
    internalprivate3*: uint32
    private4*: uint32
    private5*: bool


## Initializes an FLDictKey struct with a key string.
##         use! (The FLDictKey stores a pointer to the string, but does not copy it.)
proc init*(string: FLSlice): FLDictKey {.importc: "FLDictKey_Init".}

## Returns the string value of the key (which it was initialized with.)
proc getString*(a1: ptr FLDictKey): FLString {.importc: "FLDictKey_GetString".}

## Looks up a key in a dictionary using an FLDictKey. If the key is found, "hint" data will
##         be stored inside the FLDictKey that will speed up subsequent lookups.
proc getWithKey*(a1: FLDict; a2: ptr FLDictKey): FLValue {.importc: "FLDict_GetWithKey".}



## ////// MUTABLE DICT

## Creates a new mutable FLDict that's a copy of the source FLDict.
##         Its initial ref-count is 1, so a call to FLMutableDict_Release will free it.
##
##         Copying an immutable FLDict is very cheap (only one small allocation.) The `deepCopy` flag
##         is ignored.
##
##         Copying a mutable FLDict is cheap if it's a shallow copy, but if `deepCopy` is true,
##         nested mutable Dicts and Arrays are also copied, recursively.
##
##         If the source dict is NULL, then NULL is returned.
proc mutableCopy*(source: FLDict; a2: CopyFlags): FLMutableDict {.importc: "FLDict_MutableCopy".}

## Creates a new empty mutable FLDict.
##         Its initial ref-count is 1, so a call to FLMutableDict_Free will free it.
proc newMutableDict*(): FLMutableDict {.importc: "FLMutableDict_New".}

## Increments the ref-count of a mutable FLDict.
proc retain*(d: FLMutableDict): FLMutableDict {.inline.} =
  return cast[FLMutableDict](retain(cast[FLValue](d)))

## Decrements the refcount of (and possibly frees) a mutable FLDict.
proc release*(d: FLMutableDict) {.inline.} =
  release(cast[FLValue](d))

## If the FLDict was created by FLDict_MutableCopy, returns the original source FLDict.
proc getSource*(a1: FLMutableDict): FLDict {.importc: "FLMutableDict_GetSource".}

## Returns true if the FLDict has been changed from the source it was copied from.
proc isChanged*(a1: FLMutableDict): bool {.importc: "FLMutableDict_IsChanged".}

## Returns the FLSlot storing the key's value, adding a new one if needed (with a null value.)
##         To set the value itself, call one of the FLSlot functions, e.g. \ref FLSlot_SetInt.
proc set*(nonnull: FLMutableDict; key: FLString): FLSlot {.importc: "FLMutableDict_Set".}

## Removes the value for a key.
proc remove*(a1: FLMutableDict; key: FLString) {.importc: "FLMutableDict_Remove".}

## Removes all keys and values.
proc removeAll*(a1: FLMutableDict) {.importc: "FLMutableDict_RemoveAll".}

## Convenience function for getting an array-valued property in mutable form.
##         - If the value for the key is not an array, returns NULL.
##         - If the value is a mutable array, returns it.
##         - If the value is an immutable array, this function makes a mutable copy, assigns the
##           copy as the property value, and returns the copy.
proc getMutableArray*(a1: FLMutableDict; key: FLString): FLMutableArray {.importc: "FLMutableDict_GetMutableArray".}

## Convenience function for getting a dict-valued property in mutable form.
##         - If the value for the key is not a dict, returns NULL.
##         - If the value is a mutable dict, returns it.
##         - If the value is an immutable dict, this function makes a mutable copy, assigns the
##           copy as the property value, and returns the copy.
proc getMutableDict*(a1: FLMutableDict; key: FLString): FLMutableDict {.importc: "FLMutableDict_GetMutableDict".}



## ////// DEEP ITERATOR

##         A deep iterator traverses every value contained in a dictionary, in depth-first order.
##         You can skip any nested collection by calling FLDeepIterator_SkipChildren.
type
  DeepIterator* = ptr object  ## A reference to a deep iterator.

## Creates a FLDeepIterator to iterate over a dictionary.
##         Call FLDeepIterator_GetKey and FLDeepIterator_GetValue to get the first item,
##         then FLDeepIterator_Next.
proc newDeepIterator*(a1: FLValue): DeepIterator {.importc: "FLDeepIterator_New".}
proc free*(a1: DeepIterator) {.importc: "FLDeepIterator_Free".}

## Returns the current value being iterated over. or NULL at the end of iteration.
proc getValue*(a1: DeepIterator): FLValue {.importc: "FLDeepIterator_GetValue".}

## Returns the key of the current value, or an empty slice if not in a dictionary.
proc getKey*(a1: DeepIterator): FLSlice {.importc: "FLDeepIterator_GetKey".}

## Returns the array index of the current value, or 0 if not in an array.
proc getIndex*(a1: DeepIterator): uint32 {.importc: "FLDeepIterator_GetIndex".}

## Returns the current depth in the hierarchy, starting at 1 for the top-level children.
proc getDepth*(a1: DeepIterator): csize_t {.importc: "FLDeepIterator_GetDepth".}

## Tells the iterator to skip the children of the current value.
proc skipChildren*(a1: DeepIterator) {.importc: "FLDeepIterator_SkipChildren".}

## Advances the iterator to the next value, or returns false if at the end.
proc next*(a1: DeepIterator): bool {.importc: "FLDeepIterator_Next".}
type
  PathComponent* {.bycopy.} = object
    key*: FLSlice                ## FLDict key, or kFLSliceNull if none
    index*: uint32            ## FLArray index, only if there's no key

## Returns the path as an array of FLPathComponents.
proc getPath*(a1: DeepIterator; outPath: ptr ptr PathComponent; outDepth: ptr csize_t) {.importc: "FLDeepIterator_GetPath".}

## Returns the current path in JavaScript format.
proc getPathString*(a1: DeepIterator): FLSliceResult {.importc: "FLDeepIterator_GetPathString".}

## Returns the current path in JSONPointer format (RFC 6901).
proc getJSONPointer*(a1: DeepIterator): FLSliceResult {.importc: "FLDeepIterator_GetJSONPointer".}



## ////// PATH

##      An FLKeyPath Describes a location in a Fleece object tree, as a path from the root that follows
##      dictionary properties and array elements.
##      It's similar to a JSONPointer or an Objective-C FLKeyPath, but simpler (so far.)
##      The path is compiled into an efficient form that can be traversed quickly.
##
##      It looks like `foo.bar[2][-3].baz` -- that is, properties prefixed with a `.`, and array
##      indexes in brackets. (Negative indexes count from the end of the array.)
##
##      A leading JSONPath-like `$.` is allowed but ignored.
##
##      A '\' can be used to escape a special character ('.', '[' or '$') at the start of a
##      property name (but not yet in the middle of a name.)
##
type
  FLKeyPath* = ptr object ## A reference to a key path.

## Creates a new FLKeyPath object by compiling a path specifier string.
proc newKeyPath*(specifier: FLSlice; error: var FLError): FLKeyPath {.importc: "FLKeyPath_New".}

## Frees a compiled FLKeyPath object. (It's ok to pass NULL.)
proc free*(a1: FLKeyPath) {.importc: "FLKeyPath_Free".}

## Evaluates a compiled key-path for a given Fleece root object.
proc eval*(a1: FLKeyPath; root: FLValue): FLValue {.importc: "FLKeyPath_Eval".}

## Evaluates a key-path from a specifier string, for a given Fleece root object.
##         If you only need to evaluate the path once, this is a bit faster than creating an
##         FLKeyPath object, evaluating, then freeing it.
proc evalOnce*(specifier: FLSlice; root: FLValue; error: var FLError): FLValue {.importc: "FLKeyPath_EvalOnce".}



## ////// SHARED KEYS

proc createSharedKeys*(): SharedKeys {.importc: "FLSharedKeys_Create".}
proc retain*(a1: SharedKeys): SharedKeys {.importc: "FLSharedKeys_Retain".}
proc release*(a1: SharedKeys) {.importc: "FLSharedKeys_Release".}
proc createSharedKeysFromStateData*(a1: FLSlice): SharedKeys {.importc: "FLSharedKeys_CreateFromStateData".}
proc getStateData*(a1: SharedKeys): FLSliceResult {.importc: "FLSharedKeys_GetStateData".}
proc encode*(a1: SharedKeys; a2: FLString; add: bool): cint {.importc: "FLSharedKeys_Encode".}
proc decode*(a1: SharedKeys; key: cint): FLString {.importc: "FLSharedKeys_Decode".}
proc count*(a1: SharedKeys): cuint {.importc: "FLSharedKeys_Count".}



## ////// ENCODER

##         An FLEncoder generates encoded Fleece or JSON data. It's sort of a structured output stream,
##         with nesting. There are functions for writing every type of scalar value, and for beginning
##         and ending collections. To write a collection you begin it, write its values, then end it.
##         (Of course a value in a collection can itself be another collection.) When writing a
##         dictionary, you have to call writeKey before writing each value.
##
type
  FLEncoder* = ptr object  ## A reference to an encoder.

## Output formats a FLEncoder can generate.
type
  EncoderFormat* {.size: sizeof(cint).} = enum
    Fleece,            ## Fleece encoding
    JSON,              ## JSON encoding
    JSON5              ## [JSON5](http://json5.org), an extension of JSON with a more readable syntax


## Creates a new encoder, for generating Fleece data. Call FLEncoder_Free when done.
proc newEncoder*(): FLEncoder {.importc: "FLEncoder_New".}

## Creates a new encoder, allowing some options to be customized.
##             as a single shared value. This saves space but makes encoding slightly slower.
##             You should only turn this off if you know you're going to be writing large numbers
##             of non-repeated strings. (Default is true)
proc newEncoderWithOptions*(format: EncoderFormat; reserveSize: csize_t; uniqueStrings: bool): FLEncoder {.importc: "FLEncoder_NewWithOptions".}

## Creates a new Fleece encoder that writes to a file, not to memory.
proc newEncoderWritingToFile*(a1: ptr File; uniqueStrings: bool): FLEncoder {.importc: "FLEncoder_NewWritingToFile".}

## Frees the space used by an encoder.
proc free*(a1: FLEncoder) {.importc: "FLEncoder_Free".}

## Tells the encoder to use a shared-keys mapping when encoding dictionary keys.
proc setSharedKeys*(a1: FLEncoder; a2: SharedKeys) {.importc: "FLEncoder_SetSharedKeys".}

## Associates an arbitrary user-defined value with the encoder.
proc setExtraInfo*(a1: FLEncoder; info: pointer) {.importc: "FLEncoder_SetExtraInfo".}

## Returns the user-defined value associated with the encoder; NULL by default.
proc getExtraInfo*(a1: FLEncoder): pointer {.importc: "FLEncoder_GetExtraInfo".}

## Tells the encoder to logically append to the given Fleece document, rather than making a
##         standalone document. Any calls to FLEncoder_WriteValue() where the value points inside the
##         base data will write a pointer back to the original value.
##         The resulting data returned by FLEncoder_FinishDoc() will *NOT* be standalone; it can only
##         be used by first appending it to the base data.
##                     just create a pointer back to the original. But the encoder has to scan the
##                     base for strings first.
##                     flag. This allows them to be resolved using the `FLResolver_Begin` function,
##                     so that when the delta is used the base document can be anywhere in memory,
##                     not just immediately preceding the delta document.
proc amend*(e: FLEncoder; base: FLSlice; reuseStrings: bool; externPointers: bool) {.importc: "FLEncoder_Amend".}

## Returns the `base` value passed to FLEncoder_Amend.
proc getBase*(a1: FLEncoder): FLSlice {.importc: "FLEncoder_GetBase".}

## Tells the encoder not to write the two-byte Fleece trailer at the end of the data.
##         This is only useful for certain special purposes.
proc suppressTrailer*(a1: FLEncoder) {.importc: "FLEncoder_SuppressTrailer".}

## Resets the state of an encoder without freeing it. It can then be reused to encode
##         another value.
proc reset*(a1: FLEncoder) {.importc: "FLEncoder_Reset".}

## Returns the number of bytes encoded so far.
proc bytesWritten*(a1: FLEncoder): csize_t {.importc: "FLEncoder_BytesWritten".}

## Returns the byte offset in the encoded data where the next value will be written.
##         (Due to internal buffering, this is not the same as FLEncoder_BytesWritten.)
proc getNextWritePos*(a1: FLEncoder): csize_t {.importc: "FLEncoder_GetNextWritePos".}

##         result on error. The actual error is attached to the encoder and can be accessed by calling
##         FLEncoder_GetError or FLEncoder_End.
##
##         After an error occurs, the encoder will ignore all subsequent writes.
## Writes a `null` value to an encoder. (This is an explicitly-stored null, like the JSON
##         `null`, not the "undefined" value represented by a NULL FLValue pointer.)
proc writeNull*(a1: FLEncoder): bool {.importc: "FLEncoder_WriteNull".}

## Writes an `undefined` value to an encoder. (Its value when read will not be a `NULL`
##         pointer, but it can be recognized by `FLValue_GetType` returning `kFLUndefined`.)
##         An undefined dictionary value should be written simply by skipping the key and value.
proc writeUndefined*(a1: FLEncoder): bool {.importc: "FLEncoder_WriteUndefined".}

## Writes a boolean value (true or false) to an encoder.
proc writeBool*(a1: FLEncoder; a2: bool): bool {.importc: "FLEncoder_WriteBool".}

## Writes an integer to an encoder. The parameter is typed as `int64_t` but you can pass any
##         integral type (signed or unsigned) except for huge `uint64_t`s.
##         The number will be written in a compact form that uses only as many bytes as necessary.
proc writeInt*(a1: FLEncoder; a2: int64): bool {.importc: "FLEncoder_WriteInt".}

## Writes an unsigned integer to an encoder.
##         64-bit integers greater than or equal to 2^63, which can't be represented as int64_t.
proc writeUInt*(a1: FLEncoder; a2: uint64): bool {.importc: "FLEncoder_WriteUInt".}

## Writes a 32-bit floating point number to an encoder.
##         represented exactly as an integer, it'll be encoded as an integer to save space. This is
##         transparent to the reader, since if it requests the value as a float it'll be returned
##         as floating-point.
proc writeFloat*(a1: FLEncoder; a2: cfloat): bool {.importc: "FLEncoder_WriteFloat".}

## Writes a 64-bit floating point number to an encoder.
##         as an integer, if this can be done without losing precision. For example, 123.0 will be
##         written as an integer, and 123.75 as a float.)
proc writeDouble*(a1: FLEncoder; a2: cdouble): bool {.importc: "FLEncoder_WriteDouble".}

## Writes a string to an encoder. The string must be UTF-8-encoded and must not contain any
##         zero bytes.
proc writeString*(a1: FLEncoder; a2: FLString): bool {.importc: "FLEncoder_WriteString".}

## Writes a timestamp to an encoder, as an ISO-8601 date string.
##         metadata that distinguishes it as a date. It's just a string.)
proc writeDateString*(encoder: FLEncoder; ts: FLTimestamp; asUTC: bool): bool {.importc: "FLEncoder_WriteDateString".}

## Writes a binary data value (a blob) to an encoder. This can contain absolutely anything
##         including null bytes.
##         If the encoder is generating JSON, the blob will be written as a base64-encoded string.
proc writeData*(a1: FLEncoder; a2: FLSlice): bool {.importc: "FLEncoder_WriteData".}

## Writes raw data directly to the encoded output.
##         (This is not the same as FLEncoder_WriteData, which safely encodes a blob.)
##         it's quite unsafe, and only used for certain advanced purposes.
proc writeRaw*(a1: FLEncoder; a2: FLSlice): bool {.importc: "FLEncoder_WriteRaw".}

## Begins writing an array value to an encoder. This pushes a new state where each
##         subsequent value written becomes an array item, until FLEncoder_EndArray is called.
##             of the array, providing it here speeds up encoding slightly. If you don't know,
##             just use zero.
proc beginArray*(a1: FLEncoder; reserveCount: csize_t): bool {.importc: "FLEncoder_BeginArray".}

## Ends writing an array value; pops back the previous encoding state.
proc endArray*(a1: FLEncoder): bool {.importc: "FLEncoder_EndArray".}

## Begins writing a dictionary value to an encoder. This pushes a new state where each
##         subsequent key and value written are added to the dictionary, until FLEncoder_EndDict is
##         called.
##         Before adding each value, you must call FLEncoder_WriteKey (_not_ FLEncoder_WriteString!),
##         to write the dictionary key.
##             of the dictionary, providing it here speeds up encoding slightly. If you don't know,
##             just use zero.
proc beginDict*(a1: FLEncoder; reserveCount: csize_t): bool {.importc: "FLEncoder_BeginDict".}

## Specifies the key for the next value to be written to the current dictionary.
proc writeKey*(a1: FLEncoder; a2: FLString): bool {.importc: "FLEncoder_WriteKey".}

## Specifies the key for the next value to be written to the current dictionary.
##         The key is given as a FLValue, which must be a string or integer.
proc writeKeyValue*(a1: FLEncoder; a2: FLValue): bool {.importc: "FLEncoder_WriteKeyValue".}

## Ends writing a dictionary value; pops back the previous encoding state.
proc endDict*(a1: FLEncoder): bool {.importc: "FLEncoder_EndDict".}

## Writes a Fleece FLValue to an FLEncoder.
proc writeValue*(a1: FLEncoder; a2: FLValue): bool {.importc: "FLEncoder_WriteValue".}

## Parses JSON data and writes the object(s) to the encoder. (This acts as a single write,
##         like WriteInt; it's just that the value written is likely to be an entire dictionary of
##         array.)
proc convertJSON*(a1: FLEncoder; json: FLSlice): bool {.importc: "FLEncoder_ConvertJSON".}

## Finishes encoding the current item, and returns its offset in the output data.
proc finishItem*(a1: FLEncoder): csize_t {.importc: "FLEncoder_FinishItem".}

## Ends encoding; if there has been no error, it returns the encoded Fleece data packaged in
##         an FLDoc. (This function does not support JSON encoding.)
##         This does not free the FLEncoder; call FLEncoder_Free (or FLEncoder_Reset) next.
proc finishDoc*(a1: FLEncoder; a2: var FLError): FLDoc {.importc: "FLEncoder_FinishDoc".}

## Ends encoding; if there has been no error, it returns the encoded data, else null.
##         This does not free the FLEncoder; call FLEncoder_Free (or FLEncoder_Reset) next.
proc finish*(e: FLEncoder; outError: var FLError): FLSliceResult {.importc: "FLEncoder_Finish".}

## Returns the error code of an encoder, or NoError (0) if there's no error.
proc getError*(a1: FLEncoder): FLError {.importc: "FLEncoder_GetError".}

## Returns the error message of an encoder, or NULL if there's no error.
proc getErrorMessage*(a1: FLEncoder): cstring {.importc: "FLEncoder_GetErrorMessage".}



## ////// JSON DELTA COMPRESSION

##         These functions implement a fairly-efficient "delta" encoding that encapsulates the changes
##         needed to transform one Fleece value into another. The delta is expressed in JSON form.
##
##         A delta can be stored or transmitted
##         as an efficient way to produce the second value, when the first is already present. Deltas
##         are frequently used in version-control systems and efficient network protocols.


## Returns JSON that encodes the changes to turn the value `old` into `nuu`.
##         (The format is documented in Fleece.md, but you should treat it as a black box.)
##                     (extremely unlikely) failure.
proc createJSONDelta*(old: FLValue; nuu: FLValue): FLSliceResult {.importc: "FLCreateJSONDelta".}

## Writes JSON that describes the changes to turn the value `old` into `nuu`.
##         (The format is documented in Fleece.md, but you should treat it as a black box.)
##                 `FLEncoder_NewWithOptions`, with JSON or JSON5 format.
proc encodeJSONDelta*(old: FLValue; nuu: FLValue; jsonEncoder: FLEncoder): bool {.importc: "FLEncodeJSONDelta".}

## Applies the JSON data created by `CreateJSONDelta` to the value `old`, which must be equal
##         to the `old` value originally passed to `FLCreateJSONDelta`, and returns a Fleece document
##         equal to the original `nuu` value.
##                     equal to the `old` value used when creating the `jsonDelta`.
proc applyJSONDelta*(old: FLValue; jsonDelta: FLSlice; error: var FLError): FLSliceResult {.importc: "FLApplyJSONDelta".}

## Applies the (parsed) JSON data created by `CreateJSONDelta` to the value `old`, which must be
##         equal to the `old` value originally passed to `FLCreateJSONDelta`, and writes the corresponding
##         `nuu` value to the encoder.
##                     equal to the `old` value used when creating the `jsonDelta`.
##                     supported.)
proc encodeApplyingJSONDelta*(old: FLValue; jsonDelta: FLSlice; encoder: FLEncoder): bool {.importc: "FLEncodeApplyingJSONDelta".}

{.pop.}
