# Query

import CouchbaseLite/database
import CouchbaseLite/errors
import CouchbaseLite/fleece

from CouchbaseLite/private/cbl import nil
from CouchbaseLite/private/fl import nil

{.experimental: "notnil".}


type
    QueryObj = object
        handle: cbl.Query not nil
    Query* = ref QueryObj not nil

    QueryLanguage* {.pure.} = enum
        JSON,            ## JSON schema: github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
        N1QL             ## N1QL syntax: docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/

    QuerySyntaxError* = ref object of CouchbaseLiteError
        byteOffset: int

    Row* = object
        results: cbl.ResultSet

proc `=destroy`(d: var QueryObj) =
    cbl.release(d.handle)

proc `=`(dst: var QueryObj, src: QueryObj) {.error.} =
    echo "can't copy a query"


proc newQuery*(db: Database; str: string; language: QueryLanguage = N1QL): Query =
    var errPos: cint
    var err: cbl.Error
    let q = cbl.newQuery(db.handle, cbl.QueryLanguage(language), str, addr errPos, err)
    if q == nil:
        if err.domain == cbl.CBLDomain and err.code == int(cbl.ErrorCode.ErrorInvalidQuery):
            raise QuerySyntaxError(code: CBLErrorCode.InvalidQuery, msg: "Query syntax error", byteOffset: errPos)
        else:
            throw(err)
    else:
        return Query(handle: q)

proc explain*(query: Query): string = fl.toString(cbl.explain(query.handle))

proc columnCount*(query: Query): uint = cbl.columnCount(query.handle)

proc columnNames*(query: Query): seq[string] =
    for i in 0..query.columnCount:
        result.add( fl.toString(cbl.columnName(query.handle, cuint(i))) )

proc parameters*(query: Query): MutableDict =
    cbl.parameters(query.handle).mutableCopy()

proc `parameters=`*(query: Query, parameters: Dict) =
    cbl.setParameters(query.handle, parameters)

proc `parameters=`*(query: Query, json: string) =
    if not cbl.setParametersAsJSON(query.handle, json):
        raise errors.FleeceError(code: FleeceErrorCode.JSONError, msg: "Invalid JSON for parameters")


iterator execute*(query: Query): Row =
    var err: cbl.Error
    let rs = cbl.execute(query.handle, err)
    if rs == nil:
        throw(err)
    defer: cbl.release(rs)
    while cbl.next(rs):
        yield Row(results: rs)

proc column*(row: Row, i: uint): Value           = cbl.valueAtIndex(row.results, cuint(i))
proc column*(row: Row, name: string): Value      = cbl.valueForKey(row.results, name)
proc `[]`*(row: Row, i: uint): Value             = row.column(i)
proc `[]`*(row: Row, name: string): Value        = row.column(name)
proc asArray*(row: Row): Array                   = cbl.rowArray(row.results)
proc asDict*(row: Row): Dict                     = cbl.rowDict(row.results)
