from ._PyCBL import ffi, lib

def cstr(str):
    return ffi.new("char[]", str.encode("utf-8"))

def pystr(cstr):
    return str(ffi.string(cstr), "utf-8")

def sliceToString(s):
    if s.buf == None:
        return None
    return str(ffi.string(ffi.cast("const char*", s.buf), s.size), "utf-8")


# A global CBLError object to use in API calls, so each call doesn't have to
# allocate a new one. (This is fine as long as we're single-threaded.)
gError = ffi.new("CBLError*")


class CBLException (EnvironmentError):
    def __init__(self, message, cblError = None):
        if cblError != None:
            self.domain = cblError.domain
            self.code = cblError.code
            self.error = pystr(lib.CBLError_Message(cblError))
            EnvironmentError.__init__(self, message + ": " + self.error)
        else:
            EnvironmentError.__init__(self, message)


class CBLObject (object):
    def __init__(self, ref, message =None, error =None):
        self._ref = ref
        if not ref and message:
            raise CBLException(message, error)

    def __del__(self):
        if lib != None and "_ref" in self.__dict__ and self._ref != None:
            lib.CBL_Release(self._ref)

class ListenerToken (object):
    def __init__(self, owner, handle, c_token):
        self.owner = owner
        self.handle = handle
        self.c_token = c_token

    def __del__(self):
        self.remove()

    def remove(self):
        if self.owner != None:
            lib.CBLListener_Remove(self.c_token)
            self.owner.removeListener(self)
            self.owner = None
            self.handle = None
