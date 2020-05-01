// mod query

use super::*;
use super::base::*;
use super::c_api::*;

use std::marker::PhantomData;
use std::os::raw::c_uint;


pub enum QueryLanguage {
    JSON,   // JSON query schema: github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
    N1QL,   // N1QL syntax: docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/index.html
}


pub struct Query {
    _ref: *mut CBLQuery
}

impl Query {
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

    pub fn set_parameters(&self, parameters: MutableDict) {
        unsafe {
            CBLQuery_SetParameters(self._ref, parameters._ref);
        }
    }

    pub fn parameters(&self) -> Dict {
        unsafe { return Dict{_ref: CBLQuery_Parameters(self._ref), _owner: PhantomData}; }
    }

    pub fn set_parameters_json(&self, json: &str) {
        unsafe { CBLQuery_SetParametersAsJSON_s(self._ref, as_slice(json)); }
    }

    pub fn explain(&self) -> String {
        unsafe { CBLQuery_Explain(self._ref).to_string().unwrap() }
    }

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

    pub fn column_count(&self) -> usize {
        unsafe { CBLQuery_ColumnCount(self._ref) as usize }
    }

    pub fn column_name(&self, col: usize) -> Option<&str> {
        unsafe { CBLQuery_ColumnName(self._ref, col as u32).as_str() }
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


pub struct Row<'r> {
    results: &'r ResultSet
}

impl<'r> Row<'r> {
    pub fn get(&self, index: isize) -> Value<'r> {
        unsafe { Value{_ref: CBLResultSet_ValueAtIndex(self.results._ref, index as c_uint),
                       _owner: PhantomData} }
    }

    pub fn get_key(&self, key: &str) -> Value<'r> {
        unsafe { Value{_ref: CBLResultSet_ValueForKey_s(self.results._ref, as_slice(key)),
                       _owner: PhantomData} }
    }

    pub fn column_count(&self) -> isize {
        unsafe {
            let query = CBLResultSet_GetQuery(self.results._ref);
            return CBLQuery_ColumnCount(query) as isize;
        }
    }

    pub fn column_name(&self, col: isize) -> Option<&str> {
        unsafe {
            let query = CBLResultSet_GetQuery(self.results._ref);
            return CBLQuery_ColumnName(query, col as c_uint).as_str();
        }
    }

    pub fn columns(&self) -> Vec<Value<'r>> {
        return (0..self.column_count()).map(|i| self.get(i)).collect();
    }

    pub fn as_dict(&self) -> MutableDict {
        let mut dict = MutableDict::new();
        for i in 0..self.column_count() {
            dict.at(self.column_name(i).unwrap()).put_value(self.get(i));
        }
        return dict;
    }
}
