// A very simple program using Couchbase Lite
//
// Copyright (c) 2020 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

extern crate couchbase_lite;
extern crate tempdir;

use couchbase_lite::*;
use tempdir::TempDir;


fn main() {
    // Create a new database in a temporary directory:
    let tmp_dir = TempDir::new("cbl_rust").expect("create temp dir");
    let cfg = DatabaseConfiguration{directory: tmp_dir.path(), flags: CREATE};
    let mut db = Database::open("main_db", Some(cfg)).expect("open db");

    // Create and save a new document:
    {
        //logging::set_level(logging::Level::Info, logging::Domain::All);
        let mut doc = Document::new_with_id("foo");
        let mut props = doc.mutable_properties();
        props.at("i").put_i64(1234);
        props.at("s").put_string("Hello World!");

        db.save_document(&mut doc, ConcurrencyControl::FailOnConflict).expect("save");
    }
    // Reload the document and verify its properties:
    {
        let doc = db.get_document("foo").expect("reload document");
        let props = doc.properties();
        assert_eq!(props.to_json(), r#"{"i":1234,"s":"Hello World!"}"#);
    }
}
