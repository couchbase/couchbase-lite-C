from ._PyCBL import ffi, lib
from .common import *
from .Collections import *
import json

class Query (CBLObject):
    def __init__(self, database, jsonQuery):
        if not isinstance(jsonQuery, str):
            jsonQuery = json.dumps(jsonQuery)
        CBLObject.__init__(self,
                           lib.cbl_query_new(database._ref, cstr(jsonQuery), gError),
                           "Couldn't create query", gError)
        self.database = database
        self.columnCount = lib.cbl_query_columnCount(self._ref)
        self.jsonRepresentation = jsonQuery
        
    def __repr__(self):
        return self.__class__.__name__ + "['" + self.jsonRepresentation + "']"

    @property
    def explanation(self):
        return sliceToString(lib.cbl_query_explain(self._ref))

    @property
    def columnNames(self):
        if not "_columns" in self.__dict__:
            cols = []
            for i in range(self.columnCount):
                name = sliceToString( lib.cbl_query_columnName(self._ref, i) )
                cols.append(name)
            self._columns = cols
        return self._columns
    
    def setParameters(self, params):
        jsonStr = json.dumps(params)
        lib.cbl_query_setParametersFromJSON(self._ref, cstr(jsonStr))
    
    def execute(self):
        """Executes the query and returns a Generator of QueryResult objects."""
        results = lib.cbl_query_execute(self._ref, gError)
        if not results:
            raise CBLException("Query failed", gError)
        try:
            lastResult = None
            while lib.cbl_resultset_next(results):
                if lastResult:
                    lastResult.invalidate()
                lastResult = QueryResult(self, results)
                yield lastResult
        finally:
            lib.cbl_release(results)

class QueryResult (object):
    """A container representing a query result. It can be indexed using either
       integers (to access columns in the order they were declared in the query)
       or strings (to access columns by name.)"""
    def __init__(self, query, results):
        self.query = query
        self._ref = results
    
    def __repr__(self):
        if self._ref == None:
            return "QueryResult[invalidated]"
        return "QueryResult" + json.dumps(self.asDictionary())
        
    def invalidate(self):
        self._ref = None
        self.query = None
    
    def __len__(self):
        return self.query.columnCount
    
    def __getitem__(self, key):
        if self._ref == None:
            raise CBLException("Accessing a non-current query result row")
        if isinstance(key, int):
            if key < 0 or key >= self.query.columnCount:
                raise IndexError("Column index out of range")
            item = lib.cbl_resultset_valueAtIndex(self._ref, key)
        elif isinstance(key, str):
            item = lib.cbl_resultset_valueForKey(self._ref, key)
            if item == None:
                raise KeyError("No such column in Query")
        else:
            # TODO: Handle slices
            raise KeyError("invalid query result key")
        return decodeFleece(item)

    def __contains__(self, key):
        if self._ref == None:
            raise CBLException("Accessing a non-current query result row")
        if isinstance(key, int):
            if key < 0 or key >= self.query.columnCount:
                return False
            return (lib.cbl_results_column(self._ref, key) != None)
        elif isinstance(key, str):
            return (lib.cbl_results_property(self._ref, key) != None)
        else:
            return False
    
    def asArray(self):
        result = []
        for i in range(0, self.query.columnCount):
            result.append(self[i])
        return result
    
    def asDictionary(self):
        result = {}
        keys = self.query.columnNames
        for i in range(0, self.query.columnCount):
            item = lib.cbl_resultset_valueAtIndex(self._ref, i)
            if lib.FLValue_GetType(item) != lib.kFLUndefined:
                result[keys[i]] = decodeFleece(item)
        return result
