#! /usr/bin/env python3
#
#  test.py
#
# Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

from CouchbaseLite.Database import Database, DatabaseConfiguration
from CouchbaseLite.Document import Document, MutableDocument
from CouchbaseLite.Query import JSONQuery

Database.deleteFile("db", "/tmp")

db = Database("db", DatabaseConfiguration("/tmp"))

print ("db   = ", db)
print ("name = ", db.name)
print ("dir  = ", db.config)
print ("path = ", db.path)
print ("docs = ", db.count)

assert(db.name == "db")
assert(db.path == "/tmp/db.cblite2/")
assert(db.count == 0)

def dbListener(docIDs):
    print ("######## DB changed!", docIDs)
dbListenerToken = db.addListener(dbListener)

with db:
    doc = db.getDocument("foo")
    assert(not doc)
    doc = db.getMutableDocument("foo")
    assert(not doc)
    doc = MutableDocument("foo")
    assert(doc.id == "foo")

    print ("doc  = ", doc)

    props = doc.properties
    print ("props=", props)
    assert(props == {})

    props["flavor"] = "cardamom"
    props["numbers"] = [1, 0, 3.125]
    doc["color"] = "green"
    print ("props=", props)

    db.saveDocument(doc)

    doc2 = db.getDocument("foo")
    assert(doc2.id == "foo")
    print ("doc2 = ", doc2)
    props2 = doc2.properties
    print ("props2=", props2)
    assert(props2 == props)
    assert(doc2["color"] == "green")

    assert(db.count == 1)

    doc = MutableDocument("bar")
    doc["color"] = "green"
    doc["flavor"] = "pumpkin spice"
    db["bar"] = doc # saves it

    assert(db.count == 2)

dbListenerToken.remove()

q = JSONQuery(db, {'WHAT': [['.flavor'], ['.numbers']], 'WHERE': ['=', ['.color'], 'green']})
print ("-------- Explanation --------")
print (q.explanation)
print ("-----------------------------")
print ("Columns: ", q.columnNames)

for row in q.execute():
    print ("row: ", row.asArray(), "  ...or...  ", row.asDictionary())

db.close()
