# Fleece.nim

import Fleece_raw as fl


######## SLICES & CONVERSIONS


proc asSlice(s: FLSliceResult): fl.FLSlice =
    FLSlice(buf: s.buf, size: s.size)
proc asSlice(bytes: openarray[byte]): fl.FLSlice =
    FLSlice(buf: unsafeAddr bytes[0], size: csize_t(bytes.len))
proc asSlice(str: string): fl.FLSlice =
    FLSlice(buf: unsafeAddr str[0], size: csize_t(str.len))

proc toString(s: FLSlice): string =
    if s.buf == nil: return ""
    var str = newString(s.size)
    copyMem(addr str[0], s.buf, s.size)
    return str

proc toString(s: FLSliceResult): string =
    toString(s.asSlice())

proc toByteArray(s: FLSlice): seq[uint8] =
    var bytes = newSeq[uint8](s.size)
    copyMem(addr bytes[0], s.buf, s.size)
    return bytes


######## TYPES


type FleeceError* = ref object of CatchableError
    code: fl.FLError


type
    Value* = fl.Value
    Array* = fl.Array
    Dict*  = fl.Dict

    MutableArray* = object
        array: fl.MutableArray
    MutableDict* = object
        dict: fl.MutableDict

    ArrayObject* = Array | MutableArray
    DictObject* = Dict | MutableDict
    FleeceObject* = Value | ArrayObject | DictObject


######## FLEECE DOCUMENT


type Fleece* = object
    doc: fl.Doc

proc `=`(self: var Fleece, other: Fleece) =
    if self.doc != other.doc:
        release(self.doc)
        self.doc = other.doc
    discard retain(self.doc)

proc `=destroy`(self: var Fleece) =
    release(self.doc)


type Trust* = enum untrusted, trusted

proc parse*(data: openarray[byte], trust: Trust =untrusted): Fleece =
    let flData = asSlice(data).copy()
    let doc = fl.newDocFromResultData(flData, fl.Trust(trust), nil, FLSlice())
    if doc == nil: raise FleeceError(code: fl.FLError.InvalidData, msg: "Invalid Fleece data")
    return Fleece(doc: doc)

proc parseJSON*(json: string): Fleece =
    var err: FLError
    let doc = fl.newDocFromJSON(asSlice(json), err)
    if doc == nil: raise FleeceError(code: err, msg: "Invalid JSON")
    return Fleece(doc: doc)

proc root*(self: Fleece): Value     = self.doc.getRoot()
proc asArray*(self: Fleece): Array  = self.root.asArray()
proc asDict*(self: Fleece): Dict    = self.root.asDict()


######## VALUE / ARRAY / DICT:


type
    Type* = enum undefined = -1, null, bool, number, string, data, array, dict
    Timestamp* = fl.Timestamp
    ToJSONFlag* = enum JSON5, canonical
    ToJSONFlags* = set[ToJSONFlag]

proc asValue*(v: FleeceObject): Value   = cast[Value](v)

proc toJSON*(v: FleeceObject, flags: ToJSONFlags ={}): string =
    fl.toJSONX(v.asValue, ToJSONFlag.JSON5 in flags, ToJSONFlag.canonical in flags).toString()

proc `$`*(v: FleeceObject): string = toJSON(v, {})


######## VALUE


proc type*(v: Value): Type              = Type(fl.getType(v))

proc isNumber*(v: Value): bool          = v.type == Type.number
proc isInt*(v: Value): bool             = fl.isInteger(v)
proc isFloat*(v: Value): bool           = v.isNumber and not v.isInt
proc isString*(v: Value): bool          = v.type == Type.string

proc asBool*(v: Value): bool            = fl.asBool(v)
proc asInt64*(v: Value): int64          = fl.asInt(v)
proc asInt*(v: Value): int              = int(fl.asInt(v))
proc asFloat*(v: Value): float          = fl.asDouble(v)
proc asString*(v: Value): string        = fl.asString(v).toString()
proc asData*(v: Value): seq[uint8]      = fl.asData(v).toByteArray()
proc asTimestamp*(v: Value): Timestamp  = fl.asTimestamp(v)

proc asValue*(a: MutableArray): Value   = cast[Value](a.array)
proc asValue*(d: MutableDict): Value    = cast[Value](d.dict)

######## ARRAY

proc asArray*(a: Array): Array          = a
proc asArray*(a: MutableArray): Array   = cast[Array](a.array)
proc asArray*(v: Value): Array          = fl.asArray(v)

proc count*(a: ArrayObject): uint32     = fl.count(a.asArray)
proc isEmpty*(a: ArrayObject): bool     = fl.isEmpty(a.asArray)
proc get*(a: ArrayObject, i: uint32): Value = fl.get(a.asArray, i)

proc `[]`*(a: Array, i: uint32): Value  = a.get(i)
proc `[]`*(v: Value, i: uint32): Value  = v.asArray[i]
proc `[]`*(f: Fleece, i: uint32): Value = f.asArray[i]

iterator items*(a: ArrayObject): Value =
    var i: fl.ArrayIterator
    a.asArray.begin(addr i)
    while true:
        let v = getValue(addr i)
        if v == nil: break
        yield v
        if not next(addr i): break


######## DICT

proc asDict*(d: Dict): Dict                     = d
proc asDict*(d: MutableDict): Dict              = cast[Dict](d.dict)
proc asDict*(v: Value): Dict                    = fl.asDict(v)

proc count*(d: DictObject): uint32              = fl.count(d.asDict)
proc isEmpty*(d: DictObject): bool              = fl.isEmpty(d.asDict)
proc get*(d: DictObject, key: string): Value    = fl.get(d.asDict, key.asSlice)

proc `[]`*(d: DictObject, key: string): Value   = d.get(key)
proc `[]`*(v: Value, key: string): Value        = v.asDict[key]
proc `[]`*(f: Fleece, key: string): Value       = f.asDict[key]

iterator items*(d: DictObject): tuple [key: string, value: Value] =
    var i: fl.DictIterator
    d.asDict.begin(addr i)
    while true:
        let v = getValue(addr i)
        if v == nil: break
        let k = getKeyString(addr i).toString()
        yield (k, v)
        if not next(addr i): break


type DictKey* = object
    ## A DictKey is a cached dictionary key. It works just like a string, but it's faster because
    ## it can cache the internal representation of the key.
    ## DictKey instances have to be created by the ``dictKey`` function.
    flkey: fl.DictKey
    initialized: bool

proc dictKey*(key: string): DictKey =
    ## Creates a DictKey representing the given string.
    DictKey(flkey: fl.init(key.asSlice()), initialized: true)

proc `$`*(key: var DictKey): string =
    assert key.initialized, "Uninitialized DictKey"
    fl.getString(addr key.flkey).toString

proc get*(d: DictObject, key: var DictKey): Value =
    assert key.initialized, "Uninitialized DictKey"
    fl.getWithKey(d.asDict, addr key.flkey)

proc `[]`*(d: DictObject, key: var DictKey): Value   = d.asDict.get(key)
proc `[]`*(v: Value, key: var DictKey): Value  = v.asDict[key]
proc `[]`*(f: Fleece, key: var DictKey): Value = f.asDict[key]


type KeyPath* = object
    handle: fl.KeyPath

proc `=`(self: var KeyPath, other: KeyPath) {.error.} =
    free(self.handle)
    self.handle = other.handle

proc `=destroy`(self: var KeyPath) =
    free(self.handle)

proc keyPath*(path: string): KeyPath =
    var err: FLError
    let h = fl.newKeyPath(path.asSlice(), err)
    if h == nil: raise FleeceError(code: err, msg: "Invalid KeyPath")
    return KeyPath(handle: h)

proc eval*(path: KeyPath, v: Value): Value = path.handle.eval(v)


######## MUTABLE ARRAY


type
    CopyFlag = enum deepCopy, copyImmutables
    CopyFlags = set[CopyFlag]

proc `=`(self: var MutableArray, other: MutableArray) =
    if self.array != other.array:
        release(self.array)
        self.array = other.array
    discard retain(self.array)

proc `=destroy`(self: var MutableArray) =
    release(self.array)

proc newMutableArray*(): MutableArray =
    MutableArray(array: fl.newMutableArray())

proc mutableCopy*(a: Array, flags: CopyFlags ={}): MutableArray =
    MutableArray(array: a.mutableCopy(cast[fl.CopyFlags](flags)))

proc wrap*(a: fl.MutableArray): MutableArray =
    MutableArray(array: retain(a))

proc source*(a: MutableArray): Array    = a.array.getSource()
proc isChanged*(a: MutableArray): bool  = a.array.isChanged()

# (extra stuff for Slot to enable use of the Settable type)
proc set(s: Slot; v: string)           = set(s, v.asSlice())
proc set(s: Slot; v: openarray[uint8]) = setData(s, v.asSlice())
type Settable* = bool | int64 | uint64 | cfloat | cdouble | string | openarray[uint8] | Value

proc `[]=`*(a: MutableArray, index: uint32, value: Settable)    = a.array.set(index).set(value)
proc insert*(a: MutableArray, value: Settable, index: uint32)   = a.array.insert(index).set(value)
proc add*(a: MutableArray, value: Settable)                     = a.array.append().set(value)
proc delete*(a: MutableArray, index: uint32)                    = a.array.remove(index, 1)


######## MUTABLE DICT


proc `=`(self: var MutableDict, other: MutableDict) =
    if self.dict != other.dict:
        release(self.dict)
        self.dict = other.dict
    discard retain(self.dict)

proc `=destroy`(self: var MutableDict) =
    release(self.dict)

proc newMutableDict*(): MutableDict =
    MutableDict(dict: fl.newMutableDict())

proc mutableCopy*(d: Dict, flags: CopyFlags ={}): MutableDict =
    MutableDict(dict: d.mutableCopy(cast[fl.CopyFlags](flags)))

proc wrap*(d: fl.MutableDict): MutableDict =
    MutableDict(dict: retain(d))

proc source*(d: MutableDict): Dict      = d.dict.getSource()
proc isChanged*(d: MutableDict): bool   = d.dict.isChanged()


proc `[]=`*(d: MutableDict, key: string, value: Settable)   = d.dict.set(key.asSlice()).set(value)
proc delete*(d: MutableDict, key: string)                   = d.dict.remove(key.asSlice())
