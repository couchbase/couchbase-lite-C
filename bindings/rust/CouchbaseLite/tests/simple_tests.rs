extern crate couchbase_lite;
extern crate tempdir;

use couchbase_lite::fleece::Value;
use couchbase_lite::*;
use tempdir::TempDir;

const DB_NAME : &str = "test_db";

// Test wrapper function -- takes care of creating and deleting the database.
fn db_test<F>(f: F)
    where F: Fn(&mut Database)
{
    set_log_level(LogLevel::Warning, LogDomain::All);   // SUPPRESS LOGGING!
    
    let start_inst_count = instance_count() as isize;
    let tmp_dir = TempDir::new("cbl_rust").expect("create temp dir");
    let cfg = DatabaseConfiguration{directory: tmp_dir.path(), flags: CREATE};
    let mut db = Database::open(DB_NAME, Some(cfg)).expect("open db");
    assert!(Database::exists(DB_NAME, tmp_dir.path()));
    
    f(&mut db);
    
    drop(db);
    if instance_count() as isize > start_inst_count {
        dump_instances();
        panic!("Native object leak: {} objects, was {}", 
            instance_count(), start_inst_count);
    }
}


#[test]
fn db_properties() {
    db_test(|db| {
        assert_eq!(db.name(), DB_NAME);
        assert_eq!(db.count(), 0);
    });
}

#[test]
fn create_document() {
    db_test(|_db| {
        let doc = Document::new("foo");
        assert_eq!(doc.id(), "foo");
        assert_eq!(doc.sequence(), 0);
        assert!(doc.properties());
        assert_eq!(doc.properties().count(), 0);
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
        let doc = db.get_document("foo");
        v = doc.properties().get("a");
    }
    println!("v = {:?}", v);
}
*/