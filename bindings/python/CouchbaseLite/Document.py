# Document.py
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
from ._PyCBL import ffi, lib
from .common import *
from .Collections import *
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
        ref = lib.CBLDatabase_GetDocument(database._ref, cstr(id))
        if not ref:
            return None
        doc = Document(id)
        doc.database = database
        doc._ref = ref
        return doc

    def delete(self, concurrency = LastWriteWins):
        assert(self._ref)
        if not lib.CBLDocument_Delete(self._ref, concurrency, gError):
            raise CBLException("Couldn't delete document", gError)

    def purge(self):
        assert(self._ref)
        if not lib.CBLDocument_Purge(self._ref, gError):
            raise CBLException("Couldn't purge document", gError)

    def mutableCopy(self):
        mdoc = MutableDocument(self.id)
        mdoc.database = self.database
        mdoc._ref = lib.CBLDocument_MutableCopy(self._ref)
        return mdoc

    @property
    def sequence(self):
        if not self._ref:
            return 0
        return lib.CBLDocument_Sequence(self._ref)

    def getProperties(self):
        if not "_properties" in self.__dict__:
            if self._ref:
                fleeceProps = lib.CBLDocument_Properties(self._ref)
                self._properties = decodeFleeceDict(fleeceProps, mutable=self.isMutable)
            else:
                self._properties = {}
        return self._properties
    properties = property(getProperties)

    @property
    def JSON(self):
        return pystr(lib.CBLDocument_PropertiesAsJSON(self._ref))

    def get(self, key, dflt = None):
        return self.properties.get(key, dflt)
    def __getitem__(self, key):
        return self.properties[key]
    def __contains__(self, key):
        return key in self.properties

    def addListener(self, listener):
        self.database.addDocumentListener(listener)

    @property
    def isMutable(self):
        return False


class MutableDocument (Document):
    def __init__(self, id):
        Document.__init__(self, id)

    @staticmethod
    def _get(database, id):
        ref = lib.CBLDatabase_GetMutableDocument(database._ref, cstr(id))
        if not ref:
            return None
        doc = MutableDocument(id)
        doc.database = database
        doc._ref = ref
        return doc

    @property
    def JSON(self):
        if "_properties" in self.__dict__:
            return encodeJSON(self._properties)
        else:
            return pystr(lib.CBLDocument_PropertiesAsJSON(self._ref))

    def setProperties(self, props):
        self._properties = props
    properties = property(Document.getProperties, setProperties)

    def __setitem__(self, key, value):
        self.properties[key] = value
    def __delitem__(self, key):
        del self.properties[key]

    # Called from Database.saveDocument
    def _prepareToSave(self):
        if not self._ref:
            self._ref = lib.CBLDocument_New(cstr(self.id))
        if "_properties" in self.__dict__:
            jsonStr = encodeJSON(self._properties)
            if not lib.CBLDocument_SetPropertiesAsJSON(self._ref, cstr(jsonStr), gError):
                raise CBLException("Couldn't store properties", gError)

    def save(self, concurrency = FailOnConflict):
        return self.database.saveDocument(self, concurrency)

    @property
    def isMutable(self):
        return True
