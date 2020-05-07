# Smoke-test for Couchbase Lite Nim bindings
import Fleece
import CouchbaseLite

import strformat


proc test() =
    var config = DatabaseConfiguration(directory: "/tmp", flags: {DatabaseFlag.create})
    discard deleteDatabase("nimtest", config.directory)
    let db = openDatabase("nimtest", config)
    defer: db.close()

    var doc = db.getDocument("foo")
    if doc == nil: echo "No doc" else: echo "Found doc"

    var newDoc = newDocument("foo")
    newDoc.propertiesAsJSON = """{"language":"nim","rating":9.5}"""
    newDoc = db.saveDocument(newDoc, FailOnConflict)

    doc = db.getDocument("foo")
    echo "Read doc: ", doc.propertiesAsJSON

    echo "Properties as JSON: ", $doc.properties
    let rating = doc["rating"].asFloat
    echo "Rating = ", rating

    echo "Iterator with explicit .items:"
    for key, value in doc.properties.items:
        echo &"    '{key}' = {value}"

    echo "Iterator over pairs:"
    for kv in doc.properties:
        echo &"    '{kv.key}' = {kv.value}"

    var ratingKey = dictKey("rating")
    echo "Rating from DictKey = ", doc.properties[ratingKey]

try:
    test()
except CouchbaseLite.Error as e:
    echo "*** Error! ", $e

echo "Now forcing GC:"
GC_fullCollect()
