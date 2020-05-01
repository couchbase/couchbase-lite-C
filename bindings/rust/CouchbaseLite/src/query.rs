// mod query

use super::*;
use super::slice::*;
use super::c_api::*;

use std::marker::PhantomData;
use std::os::raw::c_uint;


/** Query languages. */
pub enum QueryLanguage {
    JSON,   // JSON query schema: github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
    N1QL,   // N1QL syntax: docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/index.html
}


/** A compiled database query. */
pub struct Query {
    _ref: *mut CBLQuery
}

impl Query {
    /** Creates a new query by compiling the input string.
        This is fast, but not instantaneous. If you need to run the same query many times, keep the
        \ref Query around instead of compiling it each time. If you need to run related queries
        with only some values different, create one query with placeholder parameter(s), and substitute
        the desired value(s) with \ref set_parameters before each time you run the query. */
    pub fn new(db: &Database, language: QueryLanguage, str: &str) -> Result<Query> {
        unsafe {
            let mut pos: i32 = 0;
            let mut err = CBLError::default();
            let q = CBLQuery_New_s(db._ref, language as CBLQueryLanguage, as_slice(str),
                                   &mut pos, &mut err);
            if q.is_null() {
                // TODO: Return the error pos somehow
                return failure(err);
            }
            return Ok(Query{_ref: q});
        }
    }

    /** Assigns values to the query's parameters.
        These values will be substited for those parameters whenever the query is executed,
        until they are next assigned.

        Parameters are specified in the query source as
        e.g. `$PARAM` (N1QL) or `["$PARAM"]` (JSON). In this example, the `parameters` dictionary
        to this call should have a key `PARAM` that maps to the value of the parameter. */
    pub fn set_parameters(&self, parameters: MutableDict) {
        unsafe {
            CBLQuery_SetParameters(self._ref, parameters._ref);
        }
    }

    /** Returns the query's current parameter bindings, if any. */
    pub fn parameters(&self) -> Dict {
        unsafe { return Dict{_ref: CBLQuery_Parameters(self._ref), _owner: PhantomData}; }
    }

    /** Assigns values to the query's parameters, from JSON data.
        See \ref set_parameters for details. */
    pub fn set_parameters_json(&self, json: &str) {
        unsafe { CBLQuery_SetParametersAsJSON_s(self._ref, as_slice(json)); }
    }

    /** Returns information about the query, including the translated SQLite form, and the search
        strategy. You can use this to help optimize the query: the word `SCAN` in the strategy
        indicates a linear scan of the entire database, which should be avoided by adding an index.
        The strategy will also show which index(es), if any, are used. */
    pub fn explain(&self) -> String {
        unsafe { CBLQuery_Explain(self._ref).to_string().unwrap() }
    }

    /** Runs the query, returning the results as a \ref ResultSet object, which is an iterator
        of \ref Row objects, each of which has column values. */
    pub fn execute(&self) -> Result<ResultSet> {
        unsafe {
            let mut err = CBLError::default();
            let r = CBLQuery_Execute(self._ref, &mut err);
            if r.is_null() {
                return failure(err);
            }
            return Ok(ResultSet{_ref: r});
        }
    }

    /** Returns the number of columns in each result.
        This comes directly from the number of "SELECT..." values in the query string. */
    pub fn column_count(&self) -> usize {
        unsafe { CBLQuery_ColumnCount(self._ref) as usize }
    }

    /** Returns the name of a column in the result.
        The column name is based on its expression in the `SELECT...` or `WHAT:` section of the
        query. A column that returns a property or property path will be named after that property.
        A column that returns an expression will have an automatically-generated name like `$1`.
        To give a column a custom name, use the `AS` syntax in the query.
        Every column is guaranteed to have a unique name. */
    pub fn column_name(&self, col: usize) -> Option<&str> {
        unsafe { CBLQuery_ColumnName(self._ref, col as u32).as_str() }
    }

    /** Returns the column names as a Vec. */
    pub fn column_names(&self) -> Vec<&str> {
        (0..self.column_count()).map(|i| self.column_name(i).unwrap()).collect()
    }
}

impl Drop for Query {
    fn drop(&mut self) {
        unsafe { release(self._ref); }
    }
}

impl Clone for Query {
    fn clone(&self) -> Self {
        unsafe { Query{_ref: retain(self._ref)} }
    }
}


//////// RESULT SET:


/** An iterator over the rows resulting from running a query. */
pub struct ResultSet {
    _ref: *mut CBLResultSet
}

impl<'r> Iterator for &'r ResultSet {
    type Item = Row<'r>;

    fn next(&mut self) -> Option<Row<'r>> {
        unsafe {
            if !CBLResultSet_Next(self._ref) {
                return None;
            }
            return Some(Row{results: &self})
        }
    }
}

impl Drop for ResultSet {
    fn drop(&mut self) {
        unsafe { release(self._ref); }
    }
}


//////// ROW:


/** A single result row from a Query. */
pub struct Row<'r> {
    results: &'r ResultSet
}

impl<'r> Row<'r> {
    /** Returns the value of a column, given its (zero-based) index. */
    pub fn get(&self, index: isize) -> Value<'r> {
        unsafe { Value{_ref: CBLResultSet_ValueAtIndex(self.results._ref, index as c_uint),
                       _owner: PhantomData} }
    }

    /** Returns the value of a column, given its name. */
    pub fn get_key(&self, key: &str) -> Value<'r> {
        unsafe { Value{_ref: CBLResultSet_ValueForKey_s(self.results._ref, as_slice(key)),
                       _owner: PhantomData} }
    }

    /** Returns the number of columns. (This is the same as \ref Query::column_count.) */
    pub fn column_count(&self) -> isize {
        unsafe {
            let query = CBLResultSet_GetQuery(self.results._ref);
            return CBLQuery_ColumnCount(query) as isize;
        }
    }

    /** Returns the name of a column. */
    pub fn column_name(&self, col: isize) -> Option<&str> {
        unsafe {
            let query = CBLResultSet_GetQuery(self.results._ref);
            return CBLQuery_ColumnName(query, col as c_uint).as_str();
        }
    }

    /** Returns all of the columns as a Fleece array. */
    pub fn as_array(&self) -> Array {
        unsafe { Array{_ref: CBLResultSet_RowArray(self.results._ref), _owner: PhantomData} }
    }

    /** Returns all of the columns as a Fleece dictionary. */
    pub fn as_dict(&self) -> Dict {
        unsafe { Dict{_ref: CBLResultSet_RowDict(self.results._ref), _owner: PhantomData} }
    }
}
