#![allow(unused_imports)]

extern crate couchbase_lite;
extern crate tempdir;

use couchbase_lite::*;
use tempdir::TempDir;

// Enables check for leaks of native CBL objects after `with_db()` finishes.
// WARNING: These checks only work if one test method runs at a time, i.e. testing is single
//          threaded. Run as `cargo test -- --test-threads=1` or you'll get false positives.
const LEAK_CHECKS : bool = true;

const DB_NAME : &str = "test_db";

const LEVEL_PREFIX : [&str;5] = ["((", "_", "", "WARNING: ", "***ERROR: "];
const LEVEL_SUFFIX : [&str;5] = ["))", "_", "", "",          " ***"];


fn logger(domain: logging::Domain, level: logging::Level, message: &str) {
    // Log to stdout, not stderr, so that `cargo test` will buffer the output.
    let i = level as usize;
    println!("CBL {:?}: {}{}{}",
             domain, LEVEL_PREFIX[i], message, LEVEL_SUFFIX[i])

}

fn init_logging() {
    logging::set_callback(Some(logger));
}

// Test wrapper function -- takes care of creating and deleting the database.
fn with_db<F>(f: F)
    where F: Fn(&mut Database)
{
    init_logging();

    let start_inst_count = instance_count() as isize;
    let tmp_dir = TempDir::new("cbl_rust").expect("create temp dir");
    let cfg = DatabaseConfiguration{directory: tmp_dir.path(), flags: CREATE};
    let mut db = Database::open(DB_NAME, Some(cfg)).expect("open db");
    assert!(Database::exists(DB_NAME, tmp_dir.path()));

    f(&mut db);

    drop(db);
    if LEAK_CHECKS && instance_count() as isize > start_inst_count {
        dump_instances();
        panic!("Native object leak: {} objects, was {}",
            instance_count(), start_inst_count);
    }
}

fn add_doc(db: &mut Database, id: &str, i: i64, s: &str) {
    let mut doc = Document::new_with_id(id);
    let mut props = doc.mutable_properties();
    props.at("i").put_i64(i);
    props.at("s").put_string(s);
    db.save_document(&mut doc, ConcurrencyControl::FailOnConflict).expect("save");
}


//////// TESTS:

#[test]
fn db_properties() {
    with_db(|db| {
        assert_eq!(db.name(), DB_NAME);
        assert_eq!(db.count(), 0);
    });
}

#[test]
fn create_document() {
    with_db(|_db| {
        let doc = Document::new_with_id("foo");
        assert_eq!(doc.id(), "foo");
        assert_eq!(doc.sequence(), 0);
        assert!(doc.properties());
        assert_eq!(doc.properties().count(), 0);
    });
}

#[test]
fn save_document() {
    with_db(|db| {
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
    });
}

#[test]
fn query() {
    with_db(|db| {
        add_doc(db, "doc-1", 1, "one");
        add_doc(db, "doc-2", 2, "two");
        add_doc(db, "doc-3", 3, "three");

        let query = Query::new(db, QueryLanguage::N1QL, "select i, s where i > 1 order by i").expect("create query");
        assert_eq!(query.column_count(), 2);
        assert_eq!(query.column_name(0), Some("i"));
        assert_eq!(query.column_name(1), Some("s"));

        // Step through the iterator manually:
        let results = query.execute().expect("execute");
        let mut row = (&results).next().unwrap(); //FIXME: Do something about the (&results). requirement
        let mut i = row.get(0).as_i64().unwrap();
        let mut s = row.get(1).as_string().unwrap();
        assert_eq!(i, 2);
        assert_eq!(s, "two");
        assert_eq!(row.as_dict().to_json(), r#"{"i":2,"s":"two"}"#);

        row = (&results).next().unwrap();
        i = row.get(0).as_i64().unwrap();
        s = row.get(1).as_string().unwrap();
        assert_eq!(i, 3);
        assert_eq!(s, "three");
        assert_eq!(row.as_dict().to_json(), r#"{"i":3,"s":"three"}"#);

        assert!((&results).next().is_none());

        // Now try a for...in loop:
        let mut n = 0;
        for row in &query.execute().expect("execute") {
            match n {
                0 => {
                    assert_eq!(row.as_array().to_json(), r#"[2,"two"]"#);
                    assert_eq!(row.as_dict().to_json(), r#"{"i":2,"s":"two"}"#);
                },
                1 => {
                    assert_eq!(row.as_array().to_json(), r#"[3,"three"]"#);
                    assert_eq!(row.as_dict().to_json(), r#"{"i":3,"s":"three"}"#);
                },
                _ => {panic!("Too many rows ({})", n);}
            }
            n += 1;

        }
        assert_eq!(n, 2);
    });
}


/*
// This test doesn't and shouldn't compile -- it tests that the borrow-checker will correctly
// prevent Fleece data from being used after its document has been freed.
#[test]
fn document_borrow_check() {
    let mut db = Database::open(DB_NAME, None).expect("open db");
    let v : Value;
    {
        let doc = db.get_document("foo").expect("get doc");
        v = doc.properties().get("a");
    }
    println!("v = {:?}", v);
}
*/
