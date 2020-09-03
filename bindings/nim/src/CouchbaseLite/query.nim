# Couchbase Lite Query class
#
# Copyright (c) 2020 Couchbase, Inc All rights reserved.
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

import CouchbaseLite/[database, errors, fleece]
import CouchbaseLite/private/cbl
import CouchbaseLite/private/fl # for slice utilities

{.experimental: "notnil".}


type
  QueryObj {.requiresInit.} = object
    handle: CBLQuery not nil
  Query* = ref QueryObj not nil
    ## A compiled database query.

  QueryLanguage* {.pure.} = enum
    ## A query language syntax.
    JSON, ## JSON schema: github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
    N1QL ## N1QL syntax: docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/

  QuerySyntaxError* = ref object of CouchbaseLiteError
    ## Exception thrown by ``Query.newQuery``. It contains the byte offset of
    ## the approximate position of the error in the input string.
    byteOffset*: int

  Row* {.requiresInit.} = object
    ## A result row from running a query. A collection that contains one
    ## "column" value for each item specified in the query's ``SELECT`` clause.
    ## The columns can be accessed by either position or name.
    results: CBLResultSet

proc `=destroy`(d: var QueryObj) =
  release(d.handle)

proc `=`(dst: var QueryObj, src: QueryObj) {.error.} =
  echo "can't copy a query"


proc newQuery*(db: Database; str: string;
    language: QueryLanguage = N1QL): Query =
  ## Creates a new Query by compiling the input string.
  ## This is fast, but not instantaneous. If you need to run the same query
  ## many times, keep the Query instance around instead of compiling it each
  ## time. If you need to run related queries with only some values different,
  ## create one query with placeholder parameter(s), and substitute the desired
  ## value(s) by setting the ``parameters`` property each time you run the
  ## query.
  var errPos: cint
  var err: CBLError
  let q = newQuery(db.internal_handle, CBLQueryLanguage(language), str,
      addr errPos, err)
  if q == nil:
    if err.domain == CBLDomain and err.code == int(
        CBLErrorCode.ErrorInvalidQuery):
      raise QuerySyntaxError(code: ErrorCode.InvalidQuery,
          msg: "Query syntax error", byteOffset: errPos)
    else:
      throw(err)
  else:
    return Query(handle: q)

proc explain*(query: Query): string =
  ## Returns information about the query, including the translated SQLite form,
  ## and the search strategy. You can use this to help optimize the query: the
  ## word ``SCAN`` in the strategy indicates a linear scan of the entire
  ## database, which should be avoided by adding an index.  The strategy will
  ## also show which index(es), if any, are used.
  query.handle.explain().toString()

proc columnCount*(query: Query): uint =
  ## The number of columns in each result ``Row``. Equal to the number of items
  ## specified in the query string's ``SELECT`` clause.
  query.handle.columnCount()

proc columnNames*(query: Query): seq[string] =
  ## The names of the result columns, in order.
  for i in 0..query.columnCount:
    result.add(query.handle.columnName(cuint(i)).toString())

proc parameters*(query: Query): Dict =
  ## The query's current parameter bindings, if any.
  query.handle.parameters()

proc `parameters=`*(query: Query, parameters: Dict) =
  ## Assigns values to the query's parameters.  These values will be substited
  ## for those parameters whenever the query is executed, until they are next
  ## assigned.
  ##
  ## A parameter named e.g. ``PARAM`` is specified in the query source as
  ## ``$PARAM`` (N1QL) or ``["$PARAM"]`` (JSON). In this example, the
  ## `parameters` dictionary to this call should have a key `PARAM` that maps
  ## to the value of the parameter.
  query.handle.setParameters(parameters)

proc `parameters=`*(query: Query, json: string) =
  ## Assigns values to the query's parameters, in the form of a JSON string
  ## containing an object whose keys are the parameter names.
  if not query.handle.setParametersAsJSON(json):
    raise errors.FleeceError(code: FleeceErrorCode.JSONError,
        msg: "Invalid JSON for parameters")


iterator execute*(query: Query): Row =
  ## Runs the query, iterating over the result rows.
  var err: CBLError
  let rs = query.handle.execute(err)
  if rs == nil:
    throw(err)
  defer: release(rs)
  while next(rs):
    yield Row(results: rs)

proc column*(row: Row, i: uint): Value =
  ## The value of a column given its (zero-based) index.
  row.results.valueAtIndex(cuint(i))

proc column*(row: Row, name: string): Value =
  ## The value of a column given its name.
  row.results.valueForKey(name)

proc `[]`*(row: Row, i: uint): Value =
  ## The value of a column given its (zero-based) index.
  row.column(i)

proc `[]`*(row: Row, name: string): Value =
  ## The value of a column given its name.
  row.column(name)

proc asArray*(row: Row): Array =
  ## An array of all the column values.
  rowArray(row.results)

proc asDict*(row: Row): Dict =
  ## A Dict of all the column names/values.
  rowDict(row.results)
