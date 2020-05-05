#! /usr/bin/env nim compile --run
# Smoke-test for generated Nim bindings

#import Fleece
import CouchbaseLite

proc test() =
    var config = DatabaseConfiguration(directory: "/tmp", flags: kDatabase_Create)

    var error : Error
    var db = openDatabase("nimtest", addr config, error)
    defer: discard db.close(error)

test()
