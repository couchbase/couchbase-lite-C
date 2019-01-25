from PyCBL import ffi, lib

def cstr(str):
    return ffi.new("char[]", str)

def sliceToString(s):
    if s.buf == None:
        return None
    return ffi.string(ffi.cast("const char*", s.buf), s.size)


# A global CBLError object to use in API calls, so each call doesn't have to
# allocate a new one. (This is fine as long as we're single-threaded.)
gError = ffi.new("CBLError*")


class CBLException (EnvironmentError):
    def __init__(self, message, cblError = None):
        if cblError != None:
            self.domain = cblError.domain
            self.code = cblError.code
            self.error = ffi.string(lib.cbl_error_message(cblError))
            EnvironmentError.__init__(self, message + ": " + self.error)
        else:
            EnvironmentError.__init__(self, message)


class CBLObject (object):
    def __init__(self, ref, message =None, error =None):
        self._ref = ref;
        if not ref and message:
            raise CBLException(message, error)

    def __del__(self):
        if lib != None and self._ref != None:
            lib.cbl_release(self._ref)

class ListenerToken (object):
    def __init__(self, owner, handle, c_token):
        self.owner = owner
        self.handle = handle
        self.c_token = c_token

    def __del__(self):
        self.remove()

    def remove(self):
        if self.owner != None:
            lib.cbl_listener_remove(self.c_token)
            self.owner.removeListener(self)
            self.owner = None
            self.handle = None

def decodeFleece(f):
    typ = lib.FLValue_GetType(f)
    if typ == lib.kFLUndefined:
        return None
    elif typ == lib.kFLNull:
        return None     # ???
    elif typ == lib.kFLBoolean:
        return not not lib.FLValue_AsBool(f)
    elif typ == lib.kFLNumber:
        if lib.FLValue_IsInteger(f):
            return lib.FLValue_AsInt(f)
        elif lib.FLValue_IsDouble(f):
            return lib.FLValue_AsDouble(f)
        else:
            return lib.FLValue_AsFloat(f)
    elif typ == lib.kFLString:
        return sliceToString(lib.FLValue_AsString(f))
    elif typ == lib.kFLArray:
        array = lib.FLValue_AsArray(f)
        result = []
        n = lib.FLArray_Count(array)
        for i in xrange(n):
            value = lib.FLArray_Get(array, i)
            result.append(decodeFleece(value))
        return result
    else:
        assert(typ == lib.kFLDict)
        result = {}
        i = ffi.new("FLDictIterator")
        lib.FLDictIterator_Begin(FLValue_AsDict(f), i)
        while True:
            value = lib.FLDictIter_GetValue(i)
            if value == None:
                break
            key = ffi.string( lib.FLDictIter_GetKeyString(i) )
            result[key] = decodeFleece(value)
            lib.FLDictIter_Next(i)
        return result
