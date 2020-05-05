// main.rs

extern crate couchbase_lite;
extern crate tempdir;

use couchbase_lite::*;
use tempdir::TempDir;

const DB_NAME : &str = "main_db";

fn main() {
    let tmp_dir = TempDir::new("cbl_rust").expect("create temp dir");
    let cfg = DatabaseConfiguration{directory: tmp_dir.path(), flags: CREATE};
    let mut db = Database::open(DB_NAME, Some(cfg)).expect("open db");
    assert!(Database::exists(DB_NAME, tmp_dir.path()));

    {
        logging::set_level(logging::Level::Info, logging::Domain::All);
        let mut doc = Document::new_with_id("foo");
        let mut props = doc.mutable_properties();
        props.at("i").put_i64(1234);
        props.at("s").put_string("Hello World!");

        db.save_document(&mut doc, ConcurrencyControl::FailOnConflict).expect("save");
    }
    {
        let doc = db.get_document("foo").expect("reload document");
        let props = doc.properties();
        verbose!("Blah blah blah");
        info!("Interesting: {} = {}", 2+2, 4);
        warn!("Some warning");
        error!("Oh no, props = {}", props);
        assert_eq!(props.to_json(), r#"{"i":1234,"s":"Hello World!"}"#);
    }
}
