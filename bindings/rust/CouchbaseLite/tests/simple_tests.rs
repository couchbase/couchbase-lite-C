extern crate couchbase_lite;
use couchbase_lite::*;

#[test]
fn smoke_test() {
    println!("Hello, world!");
    if Database::exists("rusty", "/tmp") {
        println!("The database already exists.")
    }
    let db = Database::open_in_dir("rusty", "/tmp").expect("open db");
    println!("Db name is '{}', and its path is {:?} . It contains {} documents.",
             db.name(), db.path(), db.count());
    println!("Goodbye, world!");
}
