# Query

import CouchbaseLite/database
import CouchbaseLite/errors
import CouchbaseLite/fleece

import CouchbaseLite/private/cbl
import CouchbaseLite/private/fl

{.experimental: "notnil".}


type
    QueryObj = object
        handle: CBLQuery not nil
    Query* = ref QueryObj not nil

    QueryLanguage* {.pure.} = enum
        JSON,            ## JSON schema: github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
        N1QL             ## N1QL syntax: docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/

    QuerySyntaxError* = ref object of CouchbaseLiteError
        byteOffset*: int

    Row* = object
        results: CBLResultSet

proc `=destroy`(d: var QueryObj) =
    release(d.handle)

proc `=`(dst: var QueryObj, src: QueryObj) {.error.} =
    echo "can't copy a query"


proc newQuery*(db: Database; str: string; language: QueryLanguage = N1QL): Query =
    var errPos: cint
    var err: CBLError
    let q = newQuery(db.handle, CBLQueryLanguage(language), str, addr errPos, err)
    if q == nil:
        if err.domain == CBLDomain and err.code == int(CBLErrorCode.ErrorInvalidQuery):
            raise QuerySyntaxError(code: ErrorCode.InvalidQuery, msg: "Query syntax error", byteOffset: errPos)
        else:
            throw(err)
    else:
        return Query(handle: q)

proc explain*(query: Query): string = query.handle.explain().toString()

proc columnCount*(query: Query): uint = query.handle.columnCount()

proc columnNames*(query: Query): seq[string] =
    for i in 0..query.columnCount:
        result.add( query.handle.columnName(cuint(i)).toString() )

proc parameters*(query: Query): Dict =
    query.handle.parameters()

proc `parameters=`*(query: Query, parameters: Dict) =
    query.handle.setParameters(parameters)

proc `parameters=`*(query: Query, json: string) =
    if not query.handle.setParametersAsJSON(json):
        raise errors.FleeceError(code: FleeceErrorCode.JSONError, msg: "Invalid JSON for parameters")


iterator execute*(query: Query): Row =
    var err: CBLError
    let rs = query.handle.execute(err)
    if rs == nil:
        throw(err)
    defer: release(rs)
    while next(rs):
        yield Row(results: rs)

proc column*(row: Row, i: uint): Value           = row.results.valueAtIndex(cuint(i))
proc column*(row: Row, name: string): Value      = row.results.valueForKey(name)
proc `[]`*(row: Row, i: uint): Value             = row.column(i)
proc `[]`*(row: Row, name: string): Value        = row.column(name)
proc asArray*(row: Row): Array                   = rowArray(row.results)
proc asDict*(row: Row): Dict                     = rowDict(row.results)
