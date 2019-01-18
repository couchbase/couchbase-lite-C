from PyCBL import ffi, lib
from common import *
from Document import *

class DatabaseConfiguration:
    def __init__(self, directory):
        self.directory = directory
    
    def _cblConfig(self):
        self._cblDir = cstr(self.directory)
        return ffi.new("CBLDatabaseConfiguration*", [self._cblDir])
    
    def __repr__(self):
        return "DatabaseConfiguration['" + self.directory + "']"    


class Database (CBLObject):
    def __init__(self, name, config =None):
        if config != None:
            config = config._cblConfig()
        self.name = name
        CBLObject.__init__(self, lib.cbl_db_open(name, config, gError),
                           "Couldn't open database", gError)
    
    def __repr__(self):
        return "Database['" + self.name + "']"
    
    def close(self):
        if not lib.cbl_db_close(self._ref, gError):
            print "WARNING: Database.close() failed"
        
    def delete(self):
        if not lib.cbl_db_delete(self._ref, gError):
            raise CBLException("Couldn't delete database", gError)
    
    def deleteFile(name, dir):
        if lib.cbl_deleteDB(name, dir, gError):
            return True
        elif gError.code == 0:
            return False
        else:
            raise CBLException("Couldn't delete database file", gError)
    deleteFile = staticmethod(deleteFile)
    
    def compact(self):
        if not lib.cbl_db_compact(self._ref, gError):
            raise CBLException("Couldn't compact database", gError)

    # Attributes:
        
    def getPath(self):
        return ffi.string(lib.cbl_db_path(self._ref))
    path = property(getPath)
    
    def getConfig(self):
        c_config = lib.cbl_db_config(self._ref)
        dir = ffi.string(c_config.directory)
        return DatabaseConfiguration(dir)
    config = property(getConfig)
    
    def getCount(self):
        return lib.cbl_db_count(self._ref)
    count = property(getCount)
    
    def getLastSequence(self):
        return lib.cbl_db_lastSequence(self._ref)
    lastSequence = property(getLastSequence)

    # Documents:

    def getDocument(self, id):
        return Document._get(self, id)

    def getMutableDocument(self, id):
        return MutableDocument._get(self, id)

    def save(self, doc, concurrency = FailOnConflict):
        doc._prepareToSave()
        savedDocRef = lib.cbl_db_saveDocument(self._ref, doc._ref, concurrency, gError)
        if not savedDocRef:
            raise CBLException("Couldn't save document", gError)
        savedDoc = Document(doc.id)
        savedDoc.database = self
        savedDoc._ref = savedDocRef
        return savedDoc

    def deleteDocument(self, id):
        if not lib.cbl_db_deleteDocument(self._ref, id, gError):
            raise CBLException("Couldn't delete document", gError)

    def purgeDocument(self, id):
        if not lib.cbl_db_purgeDocument(self._ref, id, gError):
            raise CBLException("Couldn't purge document", gError)

    # Batch operations:  (`with db: ...`)
    
    def __enter__(self):
        if not lib.cbl_db_beginBatch(self._ref, gError):
            raise CBLException("Couldn't begin a batch operation", gError)

    def __exit__(self, exc_type, exc_value, traceback):
        if not lib.cbl_db_endBatch(self._ref, gError) and not exc_type:
            raise CBLException("Couldn't commit a batch operation", gError)
        