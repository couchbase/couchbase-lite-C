//
// Document_Cpp.cc
//
// Copyright © 2022 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "CBLTest_Cpp.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <string>
#include <thread>

#include "cbl++/CouchbaseLite.hh"

using namespace std;
using namespace fleece;
using namespace cbl;

static constexpr const slice kCollectionCppName = "CBLTestCollectionCpp";
static constexpr const slice kOtherCollectionCppName = "CBLTestOtherCollectionCpp";

class DocumentTest_Cpp : public CBLTest_Cpp {
public:
    Collection col;
    Collection otherCol;
    
    DocumentTest_Cpp() {
        col = db.createCollection(kCollectionCppName);
        REQUIRE(col);
        CHECK(col.count() == 0);
        
        otherCol = db.createCollection(kOtherCollectionCppName);
        REQUIRE(otherCol);
        CHECK(otherCol.count() == 0);
    }
    
    MutableDocument createDocument(Collection& collection, slice docID, slice property, slice value) {
        MutableDocument doc(docID);
        doc[property] = value;
        collection.saveDocument(doc);
        return doc;
    }
};

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Missing Document", "[Document]") {
    Document doc = col.getDocument("foo");
    CHECK(!doc);
    
    MutableDocument mdoc = col.getMutableDocument("foo");
    CHECK(!mdoc);
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ New Document", "[Document]") {
    MutableDocument doc("foo");
    CHECK(doc);
    CHECK(doc.id() == "foo");
    CHECK(doc.sequence() == 0);
    CHECK(doc.properties().toJSONString() == "{}");
    CHECK(!doc.collection());

    Document immDoc = doc;
    CHECK((doc.properties() == immDoc.properties()));
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ New Document with Auto ID", "[Document]") {
    MutableDocument doc(nullptr);
    CHECK(doc);
    CHECK(!doc.id().empty());
    CHECK(doc.sequence() == 0);
    CHECK(!doc.collection());
    CHECK(doc.properties().toJSONString() == "{}");

    Document immDoc = doc;
    CHECK((doc.properties() == immDoc.properties()));
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Mutable Copy Mutable Document", "[Document]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    
    CHECK(doc.id() == "foo");
    CHECK(doc.sequence() == 0);
    CHECK(!doc.collection());
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    
    MutableDocument copiedDoc = doc.mutableCopy();
    CHECK(doc != copiedDoc);
    
    CHECK(copiedDoc.id() == "foo");
    CHECK(copiedDoc.sequence() == 0);
    CHECK(!doc.collection());
    CHECK(copiedDoc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Mutable Copy Immutable Document", "[Document]") {
    MutableDocument newdoc("foo");
    newdoc["greeting"] = "Howdy!";
    col.saveDocument(newdoc);
    CHECK(newdoc.collection() == col);
    
    Document doc = col.getDocument("foo");
    REQUIRE(doc);
    CHECK(doc.sequence() == 1);
    CHECK(doc.collection() == col);
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    
    MutableDocument copiedDoc = doc.mutableCopy();
    CHECK(doc != copiedDoc);
    
    CHECK(copiedDoc.id() == "foo");
    CHECK(copiedDoc.sequence() == 1);
    CHECK(copiedDoc.collection() == col);
    CHECK(copiedDoc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Set Properties", "[Document]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc.id() == "foo");
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
    
    MutableDict newProps = MutableDict::newDict();
    newProps["greeting"] = "Hello!";
    doc.setProperties(newProps);
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Hello!\"}");
    
    col.saveDocument(doc);
    doc = col.getMutableDocument("foo");
    CHECK(doc.id() == "foo");
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Hello!\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Get Document with Empty ID", "[Document]") {
    ExpectingExceptions x;
    Document doc = col.getDocument("");
    REQUIRE(!doc);
}

#pragma mark - Save Document:

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Save Empty Document", "[Document]") {
    MutableDocument doc("foo");
    col.saveDocument(doc);
    CHECK(doc.id()== "foo");
    CHECK(doc.sequence() == 1);
    CHECK(!doc.revisionID().empty());
    CHECK(doc.properties().toJSONString() == "{}");

    MutableDocument doc2 = col.getMutableDocument("foo");
    CHECK(doc2.id() == "foo");
    CHECK(doc2.sequence() == 1);
    CHECK(doc2.revisionID() == doc.revisionID());
    CHECK(doc2.properties().toJSONString() == "{}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Save Document With Properties", "[Document]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc["greeting"].asString() == "Howdy!");
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");

    col.saveDocument(doc);
    CHECK(doc.id() == "foo");
    CHECK(doc.sequence() == 1);
    CHECK(!doc.revisionID().empty());
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");

    MutableDocument doc2 = col.getMutableDocument("foo");
    CHECK(doc2.id() == "foo");
    CHECK(doc2.sequence() == 1);
    CHECK(doc2.revisionID() == doc.revisionID());
    CHECK(doc2.properties().toJSONString() == "{\"greeting\":\"Howdy!\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Save Document with LastWriteWin", "[Document]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc["greeting"].asString() == "Howdy!");
    REQUIRE(col.saveDocument(doc, kCBLConcurrencyControlLastWriteWins));
    CHECK(doc.sequence() == 1);
    
    MutableDocument doc1 = col.getMutableDocument("foo");
    REQUIRE(doc1);
    CHECK(doc1.id() == "foo");
    CHECK(doc1.sequence() == 1);
    
    MutableDocument doc2 = col.getMutableDocument("foo");
    REQUIRE(doc2);
    CHECK(doc2.id() == "foo");
    CHECK(doc2.sequence() == 1);

    doc1["name"] = "bob";
    REQUIRE(col.saveDocument(doc1, kCBLConcurrencyControlLastWriteWins));
    CHECK(doc1.sequence() == 2);
    
    doc2["name"] = "sally";
    REQUIRE(col.saveDocument(doc2, kCBLConcurrencyControlLastWriteWins));
    CHECK(doc2.sequence() == 3);
    
    MutableDocument doc3 = col.getMutableDocument("foo");
    CHECK(doc3.sequence() == 3);
    CHECK(doc3.properties().toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"sally\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Save Document with FailOnConflict", "[Document]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc["greeting"].asString() == "Howdy!");
    REQUIRE(col.saveDocument(doc, kCBLConcurrencyControlFailOnConflict));
    CHECK(doc.sequence() == 1);
    
    MutableDocument doc1 = col.getMutableDocument("foo");
    REQUIRE(doc1);
    CHECK(doc1.id() == "foo");
    CHECK(doc1.sequence() == 1);
    
    MutableDocument doc2 = col.getMutableDocument("foo");
    REQUIRE(doc2);
    CHECK(doc2.id() == "foo");
    CHECK(doc2.sequence() == 1);

    doc1["name"] = "bob";
    REQUIRE(col.saveDocument(doc1, kCBLConcurrencyControlFailOnConflict));
    CHECK(doc1.sequence() == 2);
    
    doc2["name"] = "sally";
    REQUIRE(!col.saveDocument(doc2, kCBLConcurrencyControlFailOnConflict));
    CHECK(doc2.sequence() == 1);
    
    doc = col.getMutableDocument("foo");
    CHECK(doc.sequence() == 2);
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"bob\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Save Document with Conflict Handler", "[Document]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    CHECK(doc["greeting"].asString() == "Howdy!");
    REQUIRE(col.saveDocument(doc, kCBLConcurrencyControlFailOnConflict));
    CHECK(doc.sequence() == 1);
    
    CollectionConflictHandler failConflict = [](MutableDocument mine, Document other) -> bool {
        return false;
    };
    
    CollectionConflictHandler mergeConflict = [](MutableDocument mine, Document other) -> bool {
        mine["anotherName"] = other["name"];
        return true;
    };
    
    MutableDocument doc1 = col.getMutableDocument("foo");
    REQUIRE(doc1);
    CHECK(doc1.id() == "foo");
    CHECK(doc1.sequence() == 1);
    
    MutableDocument doc2 = col.getMutableDocument("foo");
    REQUIRE(doc2);
    CHECK(doc2.id() == "foo");
    CHECK(doc2.sequence() == 1);

    doc1["name"] = "bob";
    REQUIRE(col.saveDocument(doc1, failConflict));
    CHECK(doc1.sequence() == 2);
    
    doc2["name"] = "sally";
    REQUIRE(!col.saveDocument(doc2, failConflict));
    CHECK(doc2.sequence() == 1);
    
    doc = col.getMutableDocument("foo");
    CHECK(doc.sequence() == 2);
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"bob\"}");
    
    doc2["name"] = "sally";
    REQUIRE(col.saveDocument(doc2, mergeConflict));
    CHECK(doc2.sequence() == 3);
    
    doc = col.getMutableDocument("foo");
    CHECK(doc.sequence() == 3);
    CHECK(doc.properties().toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"sally\",\"anotherName\":\"bob\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Save Document into Different Collection", "[Document]") {
    createDocument(col, "foo", "greeting", "Howdy");
    
    MutableDocument doc = col.getMutableDocument("foo");
    REQUIRE(doc);
    
    ExpectingExceptions ex;
    CBLError error {};
    try { otherCol.saveDocument(doc); } catch (CBLError e) { error = e; }
    CheckError(error, kCBLErrorInvalidParameter);
}

#pragma mark - Delete Document:

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Delete Non Existing Doc", "[Document]") {
    MutableDocument doc("foo");
    
    ExpectingExceptions x;
    CBLError error {};
    try { col.deleteDocument(doc); } catch (CBLError e) { error = e; }
    CheckError(error, kCBLErrorNotFound);
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Delete Doc", "[Document]") {
    createDocument(col, "foo", "greeting", "Howdy");
    
    Document doc = col.getDocument("foo");
    CHECK(doc);
    
    col.deleteDocument(doc);
    
    doc = col.getDocument("foo");
    CHECK(!doc);
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Delete Doc with LastWriteWin", "[Document]") {
    createDocument(col, "foo", "greeting", "Howdy");
    
    MutableDocument doc1 = col.getMutableDocument("foo");
    REQUIRE(doc1);
    CHECK(doc1.id() == "foo");
    CHECK(doc1.sequence() == 1);
    
    MutableDocument doc2 = col.getMutableDocument("foo");
    REQUIRE(doc2);
    CHECK(doc2.id() == "foo");
    CHECK(doc2.sequence() == 1);
    
    doc1["name"] = "bob";
    REQUIRE(col.saveDocument(doc1, kCBLConcurrencyControlLastWriteWins));
    CHECK(doc1.sequence() == 2);
    
    REQUIRE(col.deleteDocument(doc2, kCBLConcurrencyControlLastWriteWins));
    
    Document doc = col.getDocument("foo");
    REQUIRE(!doc);
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Delete Doc with FailOnConflict", "[Document]") {
    createDocument(col, "foo", "greeting", "Howdy");
    
    MutableDocument doc1 = col.getMutableDocument("foo");
    REQUIRE(doc1);
    CHECK(doc1.id() == "foo");
    CHECK(doc1.sequence() == 1);
    
    MutableDocument doc2 = col.getMutableDocument("foo");
    REQUIRE(doc2);
    CHECK(doc2.id() == "foo");
    CHECK(doc2.sequence() == 1);
    
    doc1["name"] = "bob";
    REQUIRE(col.saveDocument(doc1, kCBLConcurrencyControlFailOnConflict));
    CHECK(doc1.sequence() == 2);
    
    REQUIRE(!col.deleteDocument(doc2, kCBLConcurrencyControlFailOnConflict));
    
    Document readDoc = col.getDocument("foo");
    REQUIRE(readDoc);
    CHECK(readDoc.properties().toJSONString() == "{\"greeting\":\"Howdy\",\"name\":\"bob\"}");
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Delete Document into Different Collection", "[Document]") {
    MutableDocument doc = createDocument(col, "foo", "greeting", "Howdy");
    
    ExpectingExceptions ex;
    CBLError error {};
    try { otherCol.deleteDocument(doc); } catch (CBLError e) { error = e; }
    CheckError(error, kCBLErrorInvalidParameter);
}

#pragma mark - Purge Document:

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Purge Non Existing Doc", "[Document]") {
    MutableDocument doc("foo");
    
    ExpectingExceptions x;
    
    CBLError error {};
    try { col.purgeDocument(doc); } catch (CBLError e) { error = e; }
    CheckError(error, kCBLErrorNotFound);
    
    error = {};
    try { col.purgeDocument("foo"); } catch (CBLError e) { error = e; }
    CheckError(error, kCBLErrorNotFound);
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Purge Doc", "[Document]") {
    createDocument(col, "foo", "greeting", "Howdy");
    
    Document doc = col.getDocument("foo");
    REQUIRE(doc);
    
    SECTION("Purge with Doc") {
        col.purgeDocument(doc);
    }
    
    SECTION("Purge with ID") {
        col.purgeDocument("foo");
    }
    
    doc = col.getDocument("foo");
    CHECK(!doc);
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Purge Already Purged Document", "[Document]") {
    createDocument(col, "foo", "greeting", "Howdy");
    
    Document doc = col.getDocument("foo");
    REQUIRE(doc);
    
    col.purgeDocument(doc);
    doc = col.getDocument("foo");
    CHECK(!doc);
    
    ExpectingExceptions ex;
    CBLError error {};
    try { col.purgeDocument("foo"); } catch (CBLError e) { error = e; }
    CheckError(error, kCBLErrorNotFound);
}

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Purge Doc from Different Collection", "[Document]") {
    createDocument(col, "foo", "greeting", "Howdy");
    
    Document doc = col.getDocument("foo");
    REQUIRE(doc);
    
    ExpectingExceptions ex;
    CBLError error {};
    try { otherCol.purgeDocument(doc); } catch (CBLError e) { error = e; }
    CheckError(error, kCBLErrorInvalidParameter);
}

#pragma mark - Document Expiry:

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Document Expiration", "[Document][Expiry]") {
    createDocument(col, "doc1", "foo", "bar");
    createDocument(col, "doc2", "foo", "bar");
    createDocument(col, "doc3", "foo", "bar");

    CBLTimestamp future = CBL_Now() + 1000;
    col.setDocumentExpiration("doc1", future);
    col.setDocumentExpiration("doc3", future);
    
    CHECK(col.count() == 3);
    CHECK(col.getDocumentExpiration("doc1") == future);
    CHECK(col.getDocumentExpiration("doc3") == future);
    CHECK(col.getDocumentExpiration("doc2") == 0);
    CHECK(col.getDocumentExpiration("docx") == 0);

    this_thread::sleep_for(2000ms);
    CHECK(col.count() == 1);
}

#pragma mark - Blobs:

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Blob with Collection", "[Document][Blob]") {
    Blob blob = Blob("text/plain", "I'm Blob.");
    CHECK(blob.digest() == "sha1-FKiFNQZgW201amCeRJLKJOChjAo=");
    CHECK(blob.contentType() == "text/plain");
    CHECK(blob.length() == 9);
    
    MutableDocument doc("foo");
    doc["picture"] = blob.properties();
    col.saveDocument(doc);
    
    doc = col.getMutableDocument("foo");
    REQUIRE(doc);
    
    CHECK(doc.properties().toJSON(true,true).asString() == "{picture:{\"@type\":\"blob\","
          "content_type:\"text/plain\",digest:\"sha1-FKiFNQZgW201amCeRJLKJOChjAo=\",length:9}}");
    CHECK(Blob::isBlob(doc["picture"].asDict()));
    CHECK(blob.digest() == "sha1-FKiFNQZgW201amCeRJLKJOChjAo=");
    CHECK(blob.contentType() == "text/plain");
    CHECK(blob.length() == 9);
    CHECK(blob.loadContent() == "I'm Blob.");
}

#pragma mark - Listeners:

TEST_CASE_METHOD(DocumentTest_Cpp, "C++ Change Listeners", "[Document]") {
    int listenerCalls = 0, docListenerCalls = 0;
    
    // Add chage listener:
    auto listener = col.addChangeListener([&](CollectionChange* change) {
        ++listenerCalls;
        CHECK(change->collection() == col);
        CHECK(change->docIDs().size() == 1);
        CHECK(change->docIDs()[0] == "foo");
    });
    
    // Add doc listener:
    auto docListener = col.addDocumentChangeListener("foo", [&](DocumentChange* change) {
        ++docListenerCalls;
        CHECK(change->collection() == col);
        CHECK(change->docID() == "foo");
    });
    
    // Create a doc, check that the listener was called:
    createDocument(col, "foo", "greeting", "Howdy!");
    CHECK(listenerCalls == 1);
    CHECK(docListenerCalls == 1);
    
    // After being removed, the listener should not be called:
    listener.remove();
    docListener.remove();
    listenerCalls = docListenerCalls = 0;
    createDocument(col, "bar", "greeting", "yo.");
    
    this_thread::sleep_for(1000ms);
    CHECK(listenerCalls == 0);
    CHECK(docListenerCalls == 0);
}
