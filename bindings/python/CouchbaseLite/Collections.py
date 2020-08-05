from ._PyCBL import ffi, lib
from .common import *
from .Blob import Blob
from collections.abc import Sequence, Mapping
from functools import total_ordering
import json


FLArrayType = ffi.typeof("struct $$FLArray *")
FLDictType  = ffi.typeof("struct $$FLDict *")


#### FLEECE DECODING:


# Most general function, accepts params of type FLValue, FLDict or FLArray.
def decodeFleece(f, *, depth =1, mutable =False):
    ffitype = ffi.typeof(f)
    if ffitype == FLDictType:
        return decodeFleeceDict(f, depth=depth, mutable=mutable)
    elif ffitype == FLArrayType:
        return decodeFleeceArray(f, depth=depth, mutable=mutable)
    else:
        return decodeFleeceValue(f, depth=depth, mutable=mutable)

# Decodes an FLValue (which may of course turn out to be an FLArray or FLDict)
def decodeFleeceValue(f, *, depth =1, mutable =False):
    typ = lib.FLValue_GetType(f)
    if typ == lib.kFLString:
        return sliceToString(lib.FLValue_AsString(f))
    elif typ == lib.kFLDict:
        return decodeFleeceDict(ffi.cast(FLDictType, f), depth=depth, mutable=mutable)
    elif typ == lib.kFLArray:
        return decodeFleeceArray(ffi.cast(FLArrayType, f), depth=depth, mutable=mutable)
    elif typ == lib.kFLNumber:
        if lib.FLValue_IsInteger(f):
            return lib.FLValue_AsInt(f)
        elif lib.FLValue_IsDouble(f):
            return lib.FLValue_AsDouble(f)
        else:
            return lib.FLValue_AsFloat(f)
    elif typ == lib.kFLBoolean:
        return not not lib.FLValue_AsBool(f)
    elif typ == lib.kFLNull:
        return None     # ???
    else:
        assert(typ == lib.kFLUndefined)
        return None

# Decodes an FLArray
def decodeFleeceArray(farray, *, depth =1, mutable =False):
    if depth <= 0:
        if mutable:
            return MutableArray(fleece=farray)
        else:
            return Array(fleece=farray)
    result = []
    n = lib.FLArray_Count(farray)
    for i in range(n):
        value = lib.FLArray_Get(farray, i)
        result.append(decodeFleeceValue(value, depth=depth-1, mutable=mutable))
    return result

# Decodes an FLDict
def decodeFleeceDict(fdict, *, depth =1, mutable =False):
    if lib.CBL_IsBlob(fdict):
        return Blob(None, fdict=fdict)
    elif depth <= 0:
        if mutable:
            return MutableDictionary(fleece=fdict)
        else:
            return Dictionary(fleece=fdict)
    else:
        result = {}
        i = ffi.new("FLDictIterator*")
        lib.FLDictIterator_Begin(fdict, i)
        while True:
            value = lib.FLDictIterator_GetValue(i)
            if not value:
                break
            key = sliceToString( lib.FLDictIterator_GetKeyString(i) )
            result[key] = decodeFleeceValue(value, depth=depth-1, mutable=mutable)
            lib.FLDictIterator_Next(i)
        return result


### Array class


@total_ordering
class Array (Sequence):
    "A Couchbase Lite array, decoded from a Document or Query. Behaves like a regular Python sequence."

    def __init__(self, *, fleece=None):
        "Constructor: takes an FLArray"
        if fleece != None:
            self._flArray = fleece
        else:
            self._pyList = []

    def __len__(self):
        if not "_pyList" in self.__dict__:
            return lib.FLArray_Count(self._flArray)
        return len(self._pyList)
    
    @property
    def _toList(self):
        if not "_pyList" in self.__dict__:
            # Convert Fleece array to Python list:
            self._pyList = decodeFleeceArray(self._flArray, depth=1)
            del self._flArray
            print ("Converted Array to list")
        return self._pyList
   
    def __getitem__(self, i):
        return self._toList[i]
    
    def __repr__(self):
        if not "_pyList" in self.__dict__:
            # Don't convert in place; just return the converted form's representation
            return decodeFleeceArray(self._flArray, depth=999).__repr__()
        return self._pyList.__repr__()

    def __eq__(self, other):
        return self._toList == other
        
    def __gt__(self, other):
        return self._toList > other

    def _jsonEncodable(self):
        return self._toList


class MutableArray (Array):
    def __setitem__(self, i, value):
        self._toList.__setitem__(i, value)

    def __delitem__(self, i):
        self._toList.__deltem__(key)

    def insert(self, i, value):
        self._toList.insert(i, value)


### Dictionary class


class Dictionary (Mapping):
    "A Couchbase Lite dictionary, decoded from a Document or Query. Behaves like a regular Python mapping."
    def __init__(self, *, fleece=None):
        if fleece != None:
            self._flDict = fleece
        else:
            self._pyDict = {}

    def __len__(self):
        if not "_pyMap" in self.__dict__:
            return lib.FLDict_Count(self._flDict)
        return len(self._pyMap)

    @property
    def _toDict(self):
        if not "_pyMap" in self.__dict__:
            # Convert Fleece dict to Python mapping:
            self._pyMap = decodeFleeceDict(self._flDict)
            del self._flDict
        return self._pyMap

    def __getitem__(self, key):
        return self._toDict.__getitem__(key)

    def __iter__(self):
        return self._toDict.__iter__()
    
    def __repr__(self):
        if not "_pyMap" in self.__dict__:
            # Don't convert in place; just return the converted form's representation
            return decodeFleeceDict(self._flDict, depth=999).__repr__()
        return self._toDict.__repr__()

    def __eq__(self, other):
        return self._toDict == other
    def __ne__(self, other):
        return not self.__eq__(other)

    def _jsonEncodable(self):
        return self._toDict


class MutableDictionary (Dictionary):
    def __setitem__(self, key, value):
        self._toDict.__setitem__(key, value)

    def __delitem__(self, key):
        self._toDict.__deltem__(key)


### JSON Encoder


def encodeJSON(root, sortKeys =False):
    # Custom JSON encoding for Array, Dictionary, Blob objects
    def _defaultEncodeJSON(o):
        try:
            return o._jsonEncodable()
        except AttributeError:
            raise TypeError("Couchbase Lite documents cannot contain objects of type " + str(type(o)))
    return json.dumps(root, default=_defaultEncodeJSON, sort_keys=sortKeys, allow_nan=False)
