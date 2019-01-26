from PyCBL import ffi, lib
from common import *
import json

# Concurrency control:
LastWriteWins = 0
FailOnConflict = 1

class Document (CBLObject):
    def __init__(self, id):
        self.id = id
        self._ref = None
                        
    def __repr__(self):
        return self.__class__.__name__ + "['" + self.id + "']"
    
    @staticmethod
    def _get(database, id):
        ref = lib.cbl_db_getDocument(database._ref, id)
        if not ref:
            return None
        doc = Document(id)
        doc.database = database
        doc._ref = ref
        return doc

    def delete(self, concurrency = LastWriteWins):
        assert(self._ref)
        if not lib.cbl_doc_delete(self._ref, concurrency, gError):
            raise CBLException("Couldn't delete document", gError)
    
    def purge(self):
        assert(self._ref)
        if not lib.cbl_doc_purge(self._ref, gError):
            raise CBLException("Couldn't purge document", gError)
    
    def mutableCopy(self):
        mdoc = MutableDocument(self.id)
        mdoc.database = self.database
        mdoc._ref = lib.cbl_doc_mutableCopy(self._ref)
        return mdoc

    @property
    def sequence(self):
        if not self._ref:
            return 0
        return lib.cbl_doc_sequence(self._ref)
    
    def getProperties(self):
        if not self.__dict__.has_key("_properties"):
            if self._ref:
                fleeceProps = lib.cbl_doc_properties(self._ref)
                self._properties = decodeFleeceDict(fleeceProps)
            else:
                self._properties = {}
        return self._properties
    properties = property(getProperties)
    
    def get(self, key, dflt = None):
        return self.properties.get(key, dflt)
    def __getitem__(self, key):
        return self.properties[key]
    def __contains__(self, key):
        return key in self.properties
    

class MutableDocument (Document):
    def __init__(self, id):
        Document.__init__(self, id)

    @staticmethod
    def _get(database, id):
        ref = lib.cbl_db_getMutableDocument(database._ref, id)
        if not ref:
            return None
        doc = MutableDocument(id)
        doc.database = database
        doc._ref = ref
        return doc
    
    def setProperties(self, props):
        self._properties = props
    properties = property(Document.getProperties, setProperties)
    
    def __setitem__(self, key, value):
        self.properties[key] = value
    def __delitem__(self, key):
        del self.properties[key]
    
    def _prepareToSave(self):
        if not self._ref:
            self._ref = lib.cbl_doc_new(self.id)
        if self.__dict__.has_key("_properties"):
            jsonStr = json.dumps(self._properties)
            if not lib.cbl_doc_setPropertiesAsJSON(self._ref, jsonStr, gError):
                raise CBLException("Couldn't store properties", gError)

    def save(self, concurrency = FailOnConflict):
        return self.database.saveDocument(self, concurrency)
