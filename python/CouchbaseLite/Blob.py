from ._PyCBL import ffi, lib
from .common import *

class Blob (object):
    def __init__(self, data =None, *, db =None, dict =None):
        if dict != None:
            self._db = db
            self._dict = dict
            self.contentType = dict.get("content_type")
            self.digest = dict.get("digest")
            self.length =  dict.get("length")
        else:
            self._dict = {}
            self.contentType = None
            self.digest = None
            self.length = 0
        if data != None:
            self._data = data
    
    def __repr__(self):
        r = "Blob["
        if self.contentType != None:
            r += self.contentType
        if self.length != None:
            if self.contentType != None:
                r += ", "
                r += self.length + " bytes"
        return r + "]"
    
    @property
    def data(self):
        if "_data" in self.__dict__:
            return self._data
        if self.digest != None:
            lib.cbl_blob
    
    def _asDict(self):
