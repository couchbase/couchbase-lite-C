extern crate couchbase_lite;
extern crate tempdir;

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
    assert_eq!(instance_count() as isize - start_inst_count, 0);
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
    db_test(|db| {
        let mut doc = Document::new("foo");
        assert_eq!(doc.id(), "foo");
        assert_eq!(doc.sequence(), 0);
        assert!(doc.properties());
        assert_eq!(doc.properties().count(), 0);
    });
}
