# Database.py
#
# Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

import datetime
import math

from ._PyCBL import ffi, lib
from .common import *
from .Document import *

class DatabaseConfiguration:
    def __init__(self, directory, create = True, readOnly = False, noUpgrade = False):
        self.directory = directory
        self._flags = 0
        if create:
            self._flags |= lib.kCBLDatabase_Create
        if readOnly:
            self._flags |= lib.kCBLDatabase_ReadOnly
        if noUpgrade:
            self._flags |= lib.kCBLDatabase_NoUpgrade

    def _cblConfig(self):
        self._cblDir = cstr(self.directory)  # to keep string from being GC'd
        return ffi.new("CBLDatabaseConfiguration*", [self._cblDir, self._flags])

    def __repr__(self):
        return "DatabaseConfiguration['" + self.directory + "']"


class Database (CBLObject):
    def __init__(self, name, config =None):
        if config != None:
            config = config._cblConfig()
        self.name = name
        self.listeners = set()
        CBLObject.__init__(self, lib.CBLDatabase_Open(cstr(name), config, gError),
                           "Couldn't open database", gError)

    def __repr__(self):
        return "Database['" + self.name + "']"

    def close(self):
        if not lib.CBLDatabase_Close(self._ref, gError):
            print ("WARNING: Database.close() failed")

    def delete(self):
        if not lib.CBLDatabase_Delete(self._ref, gError):
            raise CBLException("Couldn't delete database", gError)

    @staticmethod
    def deleteFile(name, dir):
        if lib.CBL_DeleteDatabase(cstr(name), cstr(dir), gError):
            return True
        elif gError.code == 0:
            return False
        else:
            raise CBLException("Couldn't delete database file", gError)

    def compact(self):
        if not lib.CBLDatabase_Compact(self._ref, gError):
            raise CBLException("Couldn't compact database", gError)

    # Attributes:

    def getPath(self):
        return pystr(lib.CBLDatabase_Path(self._ref))
    path = property(getPath)

    @property
    def config(self):
        c_config = lib.CBLDatabase_Config(self._ref)
        dir = pystr(c_config.directory)
        return DatabaseConfiguration(dir)

    @property
    def count(self):
        return lib.CBLDatabase_Count(self._ref)

    # Documents:

    def getDocument(self, id):
        return Document._get(self, id)

    def getMutableDocument(self, id):
        return MutableDocument._get(self, id)

    def saveDocument(self, doc, concurrency = FailOnConflict):
        doc._prepareToSave()
        savedDocRef = lib.CBLDatabase_SaveDocument(self._ref, doc._ref, concurrency, gError)
        if not savedDocRef:
            raise CBLException("Couldn't save document", gError)
        savedDoc = Document(doc.id)
        savedDoc.database = self
        savedDoc._ref = savedDocRef
        return savedDoc

    def deleteDocument(self, id):
        if not lib.CBLDatabase_DeleteDocument(self._ref, cstr(id), gError):
            raise CBLException("Couldn't delete document", gError)

    def purgeDocument(self, id):
        if not lib.CBLDatabase_PurgeDocument(self._ref, cstr(id), gError):
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
        if not lib.CBLDatabase_BeginBatch(self._ref, gError):
            raise CBLException("Couldn't begin a batch operation", gError)

    def __exit__(self, exc_type, exc_value, traceback):
        if not lib.CBLDatabase_EndBatch(self._ref, gError) and not exc_type:
            raise CBLException("Couldn't commit a batch operation", gError)

    # Expiration:
    
    def getDocumentExpiration(self, id):
        exp = lib.CBLDatabase_GetDocumentExpiration(self._ref, id, gError)
        if exp > 0:
            return datetime.fromtimestamp(exp)
        elif exp == 0:
            return None
        else:
            raise CBLException("Couldn't get document's expiration", gError)
            
    def setDocumentExpiration(self, id, expDateTime):
        timestamp = 0
        if expDateTime != None:
            timestamp = math.ceil(expDateTime.timestamp)
        if not lib.CBLDatabase_SetDocumentExpiration(self._ref, id, timestamp, gError):
            raise CBLException("Couldn't set document's expiration", gError)
            

    # Listeners:

    def addListener(self, listener):
        handle = ffi.new_handle(listener)
        self.listeners.add(handle)
        c_token = lib.CBLDatabase_AddChangeListener(self._ref, lib.databaseListenerCallback, handle)
        return ListenerToken(self, handle, c_token)

    def addDocumentListener(self, docID, listener):
        handle = ffi.new_handle(listener)
        self.listeners.add(handle)
        c_token = lib.CBLDatabase_AddDocumentChangeListener(self._ref, docID,
                                                            lib.databaseListenerCallback, handle)
        return ListenerToken(self, handle, c_token)

    def removeListener(self, token):
        token.remove()


@ffi.def_extern()
def databaseListenerCallback(context, db, numDocs, c_docIDs):
    docIDs = []
    for i in range(numDocs):
        docIDs.append(pystr(c_docIDs[i]))
    listener = ffi.from_handle(context)
    listener(docIDs)

@ffi.def_extern()
def documentListenerCallback(context, db, docID):
    listener = ffi.from_handle(context)
    listener(pystr(docID))
