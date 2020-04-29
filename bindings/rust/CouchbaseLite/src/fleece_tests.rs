// mod fleece_tests

#![cfg(test)]

use crate::fleece::*;

#[test]
fn empty_values() {
    let v = Value::default();
    assert_eq!(v.get_type(), ValueType::Undefined);
    assert!(!v.is_type(ValueType::Bool));
    assert!(!v.is_number());
    assert!(!v.is_integer());
    assert_eq!(v.as_i64(), None);
    assert!(!v.as_array());
    assert!(!v.as_dict());
    assert!(!v);
    assert_eq!(v, Value::UNDEFINED);
    assert!(v == v);
}

#[test]
fn basic_values() {
    let doc = Fleece::parse_json(r#"{"i":1234,"f":12.34,"a":[1, 2],"s":"Foo"}"#).unwrap();
    let dict = doc.as_dict();
    assert_eq!(dict.count(), 4);
    
    let i = dict.get("i");
    assert!(i.is_number());
    assert!(i.is_integer());
    assert_eq!(i.as_i64(), Some(1234));
    assert_eq!(i.as_i64_or_0(), 1234);
    assert_eq!(i.as_f64(), Some(1234.0));
    assert_eq!(i.as_string(), None);
    
    let f = dict.get("f");
    assert!(f.is_number());
    assert!(!f.is_integer());
    assert_eq!(f.as_i64(), None);
    assert_eq!(f.as_i64_or_0(), 12);
    assert_eq!(f.as_f64(), Some(12.34));
    assert_eq!(f.as_string(), None);
    
    assert_eq!(dict.get("j"), Value::UNDEFINED);
    
    assert_eq!(dict.get("s").as_string(), Some("Foo"));
    
    let a = dict.get("a").as_array();
    assert!(a);
    assert_eq!(a, a);
    assert_eq!(a.count(), 2);
    assert_eq!(a.get(0).as_i64(), Some(1));
    assert_eq!(a.get(1).as_i64(), Some(2));
    assert_eq!(a.get(2).as_i64(), None);
    
    assert_eq!(doc.root().to_json(), r#"{"a":[1,2],"f":12.34,"i":1234,"s":"Foo"}"#);
    assert_eq!(format!("{}", doc.root()), r#"{"a":[1,2],"f":12.34,"i":1234,"s":"Foo"}"#);
}

/*
// This test doesn't and shouldn't compile -- it tests that the borrow-checker will correctly 
// prevent Fleece data from being used after its document has been freed. 
#[test]
fn borrow_check() {
    let v : Value;
    let str : &str;
    {
        let doc = Fleece::parse_json(r#"{"i":1234,"f":12.34,"a":[1, 2],"s":"Foo"}"#).unwrap();
        let dict = doc.as_dict();
        v = dict.get("a");
        str = dict.get("s").as_string().unwrap();
    }
    println!("v = {:?}", v);
    println!("str = {}", str);
}
*/
