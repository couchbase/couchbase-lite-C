pub mod cbl;

use cbl::*;

#[macro_use] extern crate enum_primitive;


fn main() {
    println!("Hello, world!");
    if Database::exists("rusty", "/tmp") {
        println!("The database already exists.")
    }
    let db = Database::open_in_dir("rusty", "/tmp").expect("open db");
    println!("Db name is '{}', and its path is {:?} . It contains {} documents.",
             db.name(), db.path(), db.count());
    println!("Goodbye, world!");
}
