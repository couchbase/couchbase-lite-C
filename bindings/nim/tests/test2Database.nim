# Couchbase Lite unit tests
#
# Copyright (c) 2020 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


import CouchbaseLite

import unittest

{.experimental: "notnil".}


suite "Database":
    var db: Database

    setup:
        var config = DatabaseConfiguration(directory: "/tmp", flags: {DatabaseFlag.create})
        deleteDatabase("nimtest", config.directory)
        db = openDatabase("nimtest", config)

    teardown:
        db.close()

    test "Empty db":
        check db.name == "nimtest"
        check db.path == "/tmp/nimtest.cblite2/"
        check db.count == 0
        check db.getDocument("foo") == nil

    test "Create doc":
        var newDoc = newDocument("foo")
        check newDoc.id == "foo"
        check newDoc.revisionID == ""
        check newDoc.propertiesAsJSON == "{}"
        check newDoc.properties.len == 0
        check db.count == 0

        newDoc.propertiesAsJSON = """{"language":"nim","rating":9.5}"""
        let savedDoc = db.saveDocument(newDoc, FailOnConflict)
        check savedDoc.id == "foo"
        check savedDoc.revisionID == "1-157a814d5c032e28f674c08f46722e2b07e6d879"
        check savedDoc.propertiesAsJSON == """{"language":"nim","rating":9.5}"""
        check db.count == 1

        # Load the doc from the db:
        let doc = db.getDocument("foo")
        check doc != nil
        check doc["rating"] == 9.5
        var ratingKey = dictKey("rating")
        check doc[ratingKey] == 9.5
        check doc["language"] == "nim"
        check doc["nope"] == nil

        db.deleteDocument(doc)
        check db.count == 0

    test "Save conflict":
        var newDoc = newDocument("foo")
        db.saveDocument(newDoc, FailOnConflict)

        let conflict = newDocument("foo")
        conflict.propertiesAsJSON = """{"oops":true}"""
        expect(Error):
            db.saveDocument(conflict, FailOnConflict)

    test "Modify doc":
        var newDoc = newDocument("foo")
        var props = newDoc.properties
        props["language"] = "nim"
        props["rating"] = 9.5
        var a = newMutableArray()
        a.add(123.456)
        a.add("hi")
        props["array"] = a

        newDoc = db.saveDocument(newDoc).mutableCopy()
        newDoc["rating"] = 9.6
        db.saveDocument(newDoc, FailOnConflict)

        check db["foo"].propertiesAsJSON == """{"array":[123.456,"hi"],"language":"nim","rating":9.6}"""
