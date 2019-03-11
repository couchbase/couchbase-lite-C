from ._PyCBL import ffi, lib
from .common import *
from .Document import *

class DatabaseConfiguration:
    def __init__(self, directory):
        self.directory = directory
    
    def _cblConfig(self):
        self._cblDir = cstr(self.directory)  # to keep string from being GC'd
        return ffi.new("CBLDatabaseConfiguration*", [self._cblDir])
    
    def __repr__(self):
        return "DatabaseConfiguration['" + self.directory + "']"    


class Database (CBLObject):
    def __init__(self, name, config =None):
        if config != None:
            config = config._cblConfig()
        self.name = name
        self.listeners = set()
        CBLObject.__init__(self, lib.cbl_db_open(cstr(name), config, gError),
                           "Couldn't open database", gError)
    
    def __repr__(self):
        return "Database['" + self.name + "']"
    
    def close(self):
        if not lib.cbl_db_close(self._ref, gError):
            print ("WARNING: Database.close() failed")
        
    def delete(self):
        if not lib.cbl_db_delete(self._ref, gError):
            raise CBLException("Couldn't delete database", gError)

    @staticmethod
    def deleteFile(name, dir):
        if lib.cbl_deleteDatabase(cstr(name), cstr(dir), gError):
            return True
        elif gError.code == 0:
            return False
        else:
            raise CBLException("Couldn't delete database file", gError)

    def compact(self):
        if not lib.cbl_db_compact(self._ref, gError):
            raise CBLException("Couldn't compact database", gError)

    # Attributes:
        
    def getPath(self):
        return pystr(lib.cbl_db_path(self._ref))
    path = property(getPath)

    @property
    def config(self):
        c_config = lib.cbl_db_config(self._ref)
        dir = pystr(c_config.directory)
        return DatabaseConfiguration(dir)

    @property
    def count(self):
        return lib.cbl_db_count(self._ref)

    # Documents:

    def getDocument(self, id):
        return Document._get(self, id)

    def getMutableDocument(self, id):
        return MutableDocument._get(self, id)

    def saveDocument(self, doc, concurrency = FailOnConflict):
        doc._prepareToSave()
        savedDocRef = lib.cbl_db_saveDocument(self._ref, doc._ref, concurrency, gError)
        if not savedDocRef:
            raise CBLException("Couldn't save document", gError)
        savedDoc = Document(doc.id)
        savedDoc.database = self
        savedDoc._ref = savedDocRef
        return savedDoc

    def deleteDocument(self, id):
        if not lib.cbl_db_deleteDocument(self._ref, cstr(id), gError):
            raise CBLException("Couldn't delete document", gError)

    def purgeDocument(self, id):
        if not lib.cbl_db_purgeDocument(self._ref, cstr(id), gError):
            raise CBLException("Couldn't purge document", gError)
    
    def __getitem__(self, id):
        return self.getMutableDocument(id)
    
    def __setitem__(self, id, doc):
        if id != doc.id:
            raise CBLException("key does not match document ID")
        self.saveDocument(doc)
    
    def __delitem__(self, id):
        self.deleteDocument(id)

    # Batch operations:  (`with db: ...`)
    
    def __enter__(self):
        if not lib.cbl_db_beginBatch(self._ref, gError):
            raise CBLException("Couldn't begin a batch operation", gError)

    def __exit__(self, exc_type, exc_value, traceback):
        if not lib.cbl_db_endBatch(self._ref, gError) and not exc_type:
            raise CBLException("Couldn't commit a batch operation", gError)

    # Listeners:
    
    def addListener(self, listener):
        handle = ffi.new_handle(listener)
        self.listeners.add(handle)
        c_token = lib.cbl_db_addChangeListener(self._ref, lib.databaseListenerCallback, handle)
        return ListenerToken(self, handle, c_token)

    def removeListener(self, token):
        self.listeners.remove(token.handle)
        
@ffi.def_extern()
def databaseListenerCallback(context, db, numDocs, c_docIDs):
    docIDs = []
    for i in range(numDocs):
        docIDs.append(pystr(c_docIDs[i]))
    listener = ffi.from_handle(context)
    listener(db, docIDs)
