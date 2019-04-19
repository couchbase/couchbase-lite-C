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

def dbListener(db, docIDs):
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
