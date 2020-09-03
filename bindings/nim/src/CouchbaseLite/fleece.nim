# Fleece, a JSON-like structured storage format -- https://github.com/couchbaselabs/fleece
#
# Copyright (c) 2020 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import CouchbaseLite/private/fl


## This is a Nim binding for the Fleece binary encoding format.
## Fleece documentation is here: https://github.com/couchbaselabs/fleece/wiki
## "Using Fleece" is the best starting point.

#TODO: Encoder
#TODO: DeepIterator
#TODO: JSON-Delta


#======== TYPES


type FleeceErrorCode* = enum
  ## Types of Fleece errors.
  MemoryError = 1,      ##  Out of memory, or allocation failed
  OutOfRange,           ##  Array index or iterator out of range
  InvalidData,          ##  Bad input data (NaN, non-string key, etc.)
  EncodeError,          ##  Structural error encoding (missing value, too many ends, etc.)
  JSONError,            ##  Error parsing JSON
  UnknownValue,         ##  Unparseable data in a Value (corrupt? Or from some distant future?)
  InternalError,        ##  Something that shouldn't happen
  NotFound,             ##  Key not found
  SharedKeysStateError, ##  Misuse of shared keys (not in transaction, etc.)
  POSIXError,           ##  POSIX API call failed
  Unsupported           ##  Operation is unsupported

type FleeceError* = ref object of CatchableError
  ## Exception thrown by Fleece functions.
  code*: FleeceErrorCode

proc throw(flCode: FLError; msg: string) =
  let code = cast[FleeceErrorCode](flCode)
  raise FleeceError(code: code, msg: (if msg != "": msg else: $code))


type
  Value* = FLValue ## A value of any type (JSON types or raw data.) May also be null/undefined.
  Array* = FLArray ## An array Value (unless empty/undefined)
  Dict* = FLDict ## A dictionary/map/object Value (unless empty/undefined)

  Slot* = FLSlot ## Temporary reference to a settable value in a collection

  MutableArray* = object
    ## A mutable, heap-based 'subclass' of Array.
    mval: FLMutableArray
  MutableDict* = object
    ## A mutable, heap-based 'subclass' of Dict.
    mval: FLMutableDict

  ArrayObject* = Array | MutableArray
  DictObject* = Dict | MutableDict
  ImmutableObject* = Value | Array | Dict
  MutableObject* = MutableArray | MutableDict
  FleeceObject* = ImmutableObject | MutableObject

  Settable* = bool | int64 | uint64 | cfloat | cdouble | string | openarray[
      uint8] | FleeceObject


#======== FLEECE DOCUMENT


type Fleece* = object
  ## A container that holds Fleece-encoded data and exposes its object tree.
  doc: FLDoc

proc `=`(self: var Fleece; other: Fleece) =
  if self.doc != other.doc:
    release(self.doc)
    self.doc = other.doc
  discard retain(self.doc)

proc `=destroy`(self: var Fleece) =
  release(self.doc)


type Trust* = enum
  ## Whether to trust that encoded Fleece data is valid
  untrusted, ## Data is untrusted; will be validated during parse (safer!)
  trusted    ## Data is trusted, most validation skipped (faster)

proc parse*(data: openarray[byte]; trust: Trust = untrusted): Fleece =
  ## Parses Fleece-encoded data, producing a Fleece object.
  let flData = asSlice(data).copy()
  let doc = fl.newDocFromResultData(flData, FLTrust(trust), nil, FLSlice())
  if doc == nil: throw(FLError.InvalidData, "Invalid Fleece data")
  return Fleece(doc: doc)

proc parseJSON*(json: string): Fleece =
  ## Parses JSON data, producing a Fleece object. (JSON is always assumed
  ## untrusted and will be validated. Errors cause exceptions.)
  var err: FLError
  let doc = fl.newDocFromJSON(asSlice(json), err)
  if doc == nil: throw(err, "Invalid JSON")
  return Fleece(doc: doc)

proc root*(self: Fleece): Value =
  ## The root object. This reference and all children are only valid as long as
  ## this ``Fleece`` object is.
  if self.doc == nil: nil else: self.doc.getRoot()
proc asArray*(self: Fleece): Array =
  ## The root array, or an empty/undefined value if the root is not an Array.
  ## This reference and all children are only valid as long as this ``Fleece``
  ## object is.
  self.root.asArray()
proc asDict*(self: Fleece): Dict =
  ## The root dictionary, or an empty/undefined value if the root is not a Dict.
  ## This reference and all children are only valid as long as this ``Fleece``
  ## object is.
  self.root.asDict()


#======== VALUE


proc asValue*(v: ImmutableObject): Value = cast[Value](v)
proc asValue*(a: MutableArray): Value = cast[Value](a.mval)
proc asValue*(d: MutableDict): Value = cast[Value](d.mval)

type
  Type* = enum undefined = -1, null, bool, number, string, data, array, dict
  Timestamp* = FLTimestamp

proc type*(v: Value): Type = Type(fl.getType(v))

proc isNumber*(v: Value): bool = v.type == Type.number
proc isInt*(v: Value): bool = fl.isInteger(v)
proc isFloat*(v: Value): bool = v.isNumber and not v.isInt
proc isString*(v: Value): bool = v.type == Type.string

proc asInt*[T](v: Value; dflt: T): T =
  if not v.isNumber: return dflt
  let i = fl.asInt(v)
  try:
    return T(i)
  except RangeError:
    return if i >= 0: high(T) else: low(T)

proc asBool*(v: Value): bool = fl.asBool(v)
proc asInt64*(v: Value): int64 = fl.asInt(v)
proc asInt*(v: Value): int = v.asInt(0)
proc asFloat*(v: Value): float = fl.asDouble(v)
proc asString*(v: Value): string = fl.asString(v).toString()
proc asData*(v: Value): seq[uint8] = fl.asData(v).toByteArray()
proc asTimestamp*(v: Value): Timestamp = fl.asTimestamp(v)

proc asBool*(v: Value; dflt: bool): bool =
  if v.type == Type.bool: fl.asBool(v) else: dflt
proc asFloat*(v: Value; dflt: float): float =
  if v.isNumber: v.asFloat else: dflt
proc asString*(v: Value; dflt: string): string =
  if v.isString: v.asString else: dflt
proc asTimestamp*(v: Value; dflt: Timestamp): Timestamp =
  let t = fl.asTimestamp(v)
  if ord(t) > 0: t else: dflt

proc `==`*(v: Value; n: int64): bool = v.isInt and v.asInt == n
proc `==`*(v: Value; n: float64): bool = v.isNumber and v.asFloat == n
proc `==`*(v: Value; str: string): bool = v.isString and v.asString == str

type
  ToJSONFlag* = enum JSON5, canonical
  ToJSONFlags* = set[ToJSONFlag]

proc toJSON*(v: FleeceObject; flags: ToJSONFlags = {}): string =
  ## Converts any value to a JSON string. Returns "undefined" if the value is
  ## undefined.
  fl.toJSONX(v.asValue, ToJSONFlag.JSON5 in flags, ToJSONFlag.canonical in
      flags).toString()

proc `$`*(v: FleeceObject): string = toJSON(v, {})

#======== ARRAY

proc asArray*(a: Array): Array = a
proc asArray*(a: MutableArray): Array = cast[Array](a.mval)
proc asArray*(v: Value): Array = fl.asArray(v)

proc len*(a: ArrayObject): uint32 = fl.count(a.asArray)
proc isEmpty*(a: ArrayObject): bool = fl.isEmpty(a.asArray)
proc get*(a: ArrayObject; i: uint32): Value = fl.get(a.asArray, i)

proc `[]`*(a: Array; i: uint32): Value = a.get(i)
proc `[]`*(v: Value; i: uint32): Value = v.asArray[i]
proc `[]`*(f: Fleece; i: uint32): Value = f.asArray[i]

iterator items*(a: ArrayObject): Value =
  ## Array iterator.
  var i: FLArrayIterator
  a.asArray.begin(addr i)
  while true:
    let v = getValue(addr i)
    if v == nil: break
    yield v
    if not next(addr i): break


#======== DICT

proc asDict*(d: Dict): Dict = d
proc asDict*(d: MutableDict): Dict = cast[Dict](d.mval)
proc asDict*(v: Value): Dict = fl.asDict(v)

proc len*(d: DictObject): uint32 = fl.count(d.asDict)
proc isEmpty*(d: DictObject): bool = fl.isEmpty(d.asDict)
proc get*(d: DictObject; key: string): Value = fl.get(d.asDict, key.asSlice)

proc `[]`*(d: DictObject; key: string): Value = d.get(key)
proc `[]`*(v: Value; key: string): Value = v.asDict[key]
proc `[]`*(f: Fleece; key: string): Value = f.asDict[key]

iterator items*(d: DictObject): tuple [key: string; value: Value] =
  ## Dict iterator.
  var i: FLDictIterator
  d.asDict.begin(addr i)
  while true:
    let v = getValue(addr i)
    if v == nil: break
    let k = getKeyString(addr i).toString()
    yield (k, v)
    if not next(addr i): break


type DictKey* {.requiresInit.} = object
  ## A DictKey is a cached dictionary key. It works just like a string, but it's
  ## faster because it can cache the internal representation of the key.
  ## DictKey instances have to be created by the ``dictKey`` function.
  flkey: FLDictKey

proc dictKey*(key: string): DictKey =
  ## Creates a DictKey representing the given string.
  DictKey(flkey: fl.init(key.asSlice()))

proc `$`*(key: var DictKey): string =
  fl.getString(addr key.flkey).toString

proc get*(d: DictObject; key: var DictKey): Value =
  fl.getWithKey(d.asDict, addr key.flkey)

proc `[]`*(d: DictObject; key: var DictKey): Value = d.asDict.get(key)
proc `[]`*(v: Value; key: var DictKey): Value = v.asDict[key]
proc `[]`*(f: Fleece; key: var DictKey): Value = f.asDict[key]


type KeyPath* {.requiresInit.} = object
  ## KeyPath is like a multi-level DictKey: it's initialized with a path string
  ## like "coords.lat" or "contacts[2].lastName" and can then be used like a key
  ## to access the value at that path in any object. If any component in the
  ## path is missing, the result is a null/undefined Value.
  handle: FLKeyPath

proc `=`(self: var KeyPath; other: KeyPath) {.error.} =
  free(self.handle)
  self.handle = other.handle

proc `=destroy`(self: var KeyPath) =
  free(self.handle)

proc keyPath*(path: string): KeyPath =
  ## Creates a new KeyPath from a path string like "coords.lat" or
  ## "contacts[2].lastName".  Throws an exception if the path syntax is invalid.
  var err: FLError
  let h = fl.newKeyPath(path.asSlice(), err)
  if h == nil: throw(err, "Invalid KeyPath")
  return KeyPath(handle: h)

proc eval*(path: KeyPath; v: Value): Value =
  path.handle.eval(v)


#======== MUTABLE ARRAY


type
  CopyFlag = enum  ## Options for making (mutable) copies of Fleece values.
    deepCopy,      ## Copy the entire object tree from here
    copyImmutables ## Even copy immutable values from Fleece data
  CopyFlags = set[CopyFlag]

proc `=`(self: var MutableArray; other: MutableArray) =
  if self.mval != other.mval:
    release(self.mval)
    self.mval = other.mval
  discard retain(self.mval)

proc `=destroy`(self: var MutableArray) =
  release(self.mval)

proc newMutableArray*(): MutableArray =
  ## Creates an empty mutable array. Unlike the immutable values that point into
  ## Fleece-formatted data, this object is not dependent on a container and
  ## stays valid as long as you have a reference to it.
  MutableArray(mval: fl.newMutableArray())

proc mutableCopy*(a: Array; flags: CopyFlags = {}): MutableArray =
  ## Makes a mutable copy of an array.
  MutableArray(mval: a.mutableCopy(cast[fl.CopyFlags](flags)))

proc wrap*(a: FLMutableArray): MutableArray =
  MutableArray(mval: retain(a))

proc source*(a: MutableArray): Array =
  ## If this is a mutable copy of an immutable Array, returns the original.
  a.mval.getSource()
proc isChanged*(a: MutableArray): bool =
  ## Returns true if this array has been modified since created.
  a.mval.isChanged()

# (extra stuff for Slot to enable use of the Settable type)
proc set(s: Slot; v: string) = set(s, v.asSlice())
proc set(s: Slot; v: openarray[uint8]) = setData(s, v.asSlice())
proc set(s: Slot; v: FleeceObject) = set(s, v.asValue())

proc set*(a: MutableArray; index: uint32; value: Settable) =
  ## Stores a value at an existing (zero-based) index in a mutable array.
  a.mval.set(index).set(value)
proc `[]=`*(a: MutableArray; index: uint32; value: Settable) =
  ## Stores a value at an existing (zero-based) index in a mutable array.
  a.mval.set(index).set(value)
proc insert*(a: MutableArray; value: Settable; index: uint32) =
  ## Inserts a value at a (zero-based) index in a mutable array.
  ## Any items at that index and higher are pushed up by one.
  a.mval.insert(index).set(value)
proc add*(a: MutableArray; value: Settable) =
  ## Appends a value to the end of a mutable array.
  a.mval.append().set(value)
proc delete*(a: MutableArray; index: uint32) =
  ## Removes a value from a mutable array. Any items at higher indexes move down
  ## one.
  a.mval.remove(index, 1)


#======== MUTABLE DICT


proc `=`(self: var MutableDict; other: MutableDict) =
  if self.mval != other.mval:
    release(self.mval)
    self.mval = other.mval
  discard retain(self.mval)

proc `=destroy`(self: var MutableDict) =
  release(self.mval)

proc newMutableDict*(): MutableDict =
  ## Creates an empty mutable dictionary. Unlike the immutable values that point
  ## into Fleece-formatted data, this object is not dependent on a container and
  ## stays valid as long as you have a reference to it.
  MutableDict(mval: fl.newMutableDict())

proc mutableCopy*(d: Dict; flags: CopyFlags = {}): MutableDict =
  ## Makes a mutable copy of a dictionary.
  MutableDict(mval: d.mutableCopy(cast[fl.CopyFlags](flags)))

proc wrap*(d: FLMutableDict): MutableDict =
  MutableDict(mval: retain(d))

proc source*(d: MutableDict): Dict =
  ## If this is a mutable copy of an immutable Dict, returns the original.
  d.mval.getSource()
proc isChanged*(d: MutableDict): bool =
  ## Returns true if this array has been modified since created.
  d.mval.isChanged()

proc set*(d: MutableDict; key: string; value: Settable) =
  ## Stores a value for a key in a mutable dictionary.
  ## Any prior value for that key is replaced.
  d.mval.set(key.asSlice()).set(value)
proc `[]=`*(d: MutableDict; key: string; value: Settable) =
  ## Stores a value for a key in a mutable dictionary.
  ## Any prior value for that key is replaced.
  d.mval.set(key.asSlice()).set(value)
proc delete*(d: MutableDict; key: string) =
  ## Removes the value, if any, for a key in a mutable dictionary.
  d.mval.remove(key.asSlice())
