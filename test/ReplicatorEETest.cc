//
// ReplicatorEETest.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
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

#include "ReplicatorTest.hh"
#include "CBLPrivate.h"


#ifdef COUCHBASE_ENTERPRISE     // Local-to-local replication is an EE feature


class ReplicatorLocalTest : public ReplicatorTest {
public:
    Database otherDB;

    ReplicatorLocalTest()
    :otherDB(openEmptyDatabaseNamed("otherDB"))
    {
        config.endpoint = CBLEndpoint_NewWithLocalDB(otherDB.ref());
        config.replicatorType = kCBLReplicatorTypePush;
    }
};


TEST_CASE_METHOD(ReplicatorLocalTest, "Push to local db", "[Replicator]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);

    replicate();

    CHECK(asVector(docsNotified) == vector<string>{"foo"});

    Document copiedDoc = otherDB.getDocument("foo");
    REQUIRE(copiedDoc);
    CHECK(copiedDoc["greeting"].asString() == "Howdy!"_sl);
}


TEST_CASE_METHOD(ReplicatorLocalTest, "Pull conflict (default resolver)", "[Replicator][Conflict]") {
    config.replicatorType = kCBLReplicatorTypePull;

    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);

    MutableDocument doc2("foo");
    doc2["greeting"] = "Salaam Alaykum";
    otherDB.saveDocument(doc2);

    replicate();

    CHECK(asVector(docsNotified) == vector<string>{"foo"});

    Document copiedDoc = db.getDocument("foo");
    REQUIRE(copiedDoc);
    CHECK(copiedDoc["greeting"].asString() == "Howdy!"_sl);
}


class ReplicatorConflictTest : public ReplicatorLocalTest {
public:

    bool deleteLocal {false}, deleteRemote {false}, deleteMerged {false};
    bool resolverCalled {false};

    alloc_slice expectedLocalRevID, expectedRemoteRevID;

    void testConflict(bool delLocal, bool delRemote, bool delMerged) {
        deleteLocal = delLocal;
        deleteRemote = delRemote;
        deleteMerged = delMerged;

        config.replicatorType = kCBLReplicatorTypePull;

        // Save the same doc to each db (will have the same revision),
        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        db.saveDocument(doc);
        if (deleteLocal) {
            db.deleteDocument(doc);
        } else {
            doc["expletive"] = "Shazbatt!";
            db.saveDocument(doc);
            expectedLocalRevID = doc.revisionID();
        }

        doc = MutableDocument("foo");
        doc["greeting"] = "Howdy!";
        otherDB.saveDocument(doc);
        if (deleteRemote) {
            otherDB.deleteDocument(doc);
        } else {
            doc["expletive"] = "Frak!";
            otherDB.saveDocument(doc);
            expectedRemoteRevID = CBLDocument_CanonicalRevisionID(doc.ref());
        }

        config.conflictResolver = [](void *context,
                                     FLString documentID,
                                     const CBLDocument *localDocument,
                                     const CBLDocument *remoteDocument) -> const CBLDocument* {
            cerr << "--- Entering custom conflict resolver! (local=" << localDocument <<
                    ", remote=" << remoteDocument << ")\n";
            auto merged = ((ReplicatorConflictTest*)context)->conflictResolver(documentID, localDocument, remoteDocument);
            cerr << "--- Returning " << merged << " from custom conflict resolver\n";
            return merged;
        };

        replicate();

        CHECK(resolverCalled);
        CHECK(asVector(docsNotified) == vector<string>{"foo"});

        Document copiedDoc = db.getDocument("foo");
        if (deleteMerged) {
            REQUIRE(!copiedDoc);
        } else {
            REQUIRE(copiedDoc);
            CHECK(copiedDoc["greeting"].asString() == "¡Hola!"_sl);
        }
    }


    const CBLDocument* conflictResolver(slice documentID,
                                        const CBLDocument *localDocument,
                                        const CBLDocument *remoteDocument)
    {
        CHECK(!resolverCalled);
        resolverCalled = true;

        CHECK(string(documentID) == "foo");
        if (deleteLocal) {
            REQUIRE(!localDocument);
            REQUIRE(!expectedLocalRevID);
        } else {
            REQUIRE(localDocument);
            CHECK(CBLDocument_ID(localDocument) == "foo"_sl);
            CHECK(slice(CBLDocument_RevisionID(localDocument)) == expectedLocalRevID);
            Dict localProps(CBLDocument_Properties(localDocument));
            CHECK(localProps["greeting"].asString() == "Howdy!"_sl);
            CHECK(localProps["expletive"].asString() == "Shazbatt!"_sl);
        }
        if (deleteRemote) {
            REQUIRE(!remoteDocument);
            REQUIRE(!expectedRemoteRevID);
        } else {
            REQUIRE(remoteDocument);
            CHECK(CBLDocument_ID(remoteDocument) == "foo"_sl);
            CHECK(slice(CBLDocument_RevisionID(remoteDocument)) == expectedRemoteRevID);
            Dict remoteProps(CBLDocument_Properties(remoteDocument));
            CHECK(remoteProps["greeting"].asString() == "Howdy!"_sl);
            CHECK(remoteProps["expletive"].asString() == "Frak!"_sl);
        }
        if (deleteMerged) {
            return nullptr;
        } else {
            CBLDocument *merged = CBLDocument_CreateWithID(documentID);
            MutableDict mergedProps(CBLDocument_MutableProperties(merged));
            mergedProps.set("greeting"_sl, "¡Hola!");
            // do not release `merged`, otherwise it would be freed before returning!
            return merged;
        }
    }
};


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict (custom resolver)",
                 "[Replicator][Conflict]") {
    testConflict(false, false, false);
}


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict with remote deletion (custom resolver)",
                 "[Replicator][Conflict]") {
    testConflict(false, true, false);
}


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict with local deletion (custom resolver)",
                 "[Replicator][Conflict]") {
    testConflict(true, false, false);
}


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict deleting merge (custom resolver)",
                 "[Replicator][Conflict]") {
    testConflict(false, true, true);
}


class ReplicatorFilterTest : public ReplicatorLocalTest {
public:
    int count = 0;
    int deletedCount = 0;
    alloc_slice deletedDocID;
    
    void testFilter(bool isPush) {
        cbl::Database theDB;
        
        if (isPush) {
            theDB = db;
            config.replicatorType = kCBLReplicatorTypePush;
            config.pushFilter = [](void *context, CBLDocument* doc, CBLDocumentFlags flags) -> bool {
                ReplicatorFilterTest* test = ((ReplicatorFilterTest*)context);
                return test->filter(context, doc, flags);
            };
        } else {
            theDB = otherDB;
            config.replicatorType = kCBLReplicatorTypePull;
            config.pullFilter = [](void *context, CBLDocument* doc, CBLDocumentFlags flags) -> bool {
                ReplicatorFilterTest* test = ((ReplicatorFilterTest*)context);
                return test->filter(context, doc, flags);
            };
            
            // Make local db non-empty for pulling a deleted doc:
            MutableDocument doc0("doc0");
            db.saveDocument(doc0);
        }
        
        MutableDocument doc1("doc1");
        theDB.saveDocument(doc1);
        
        MutableDocument doc2("doc2");
        theDB.saveDocument(doc2);
        
        MutableDocument doc3("doc3");
        theDB.saveDocument(doc3);
        theDB.deleteDocument(doc3);
        
        replicate();
        
        CHECK(count == 3);
        CHECK(deletedCount == 1);
        CHECK(deletedDocID == "doc3"_sl);
    }
    
    bool filter(void *context, CBLDocument* doc, CBLDocumentFlags flags) {
        count++;
        
        if (flags & kCBLDocumentFlagsDeleted) {
            deletedCount++;
            deletedDocID = CBLDocument_ID(doc);
        }
        
        if (FLSlice_Equal(CBLDocument_ID(doc), "doc2"_sl))
            return false;
        
        return true;
    }
};


TEST_CASE_METHOD(ReplicatorFilterTest, "Push Filter", "[Replicator][Filter]") {
    testFilter(true);
}


TEST_CASE_METHOD(ReplicatorFilterTest, "Pull Filter", "[Replicator][Filter]") {
    testFilter(false);
}


#endif // COUCHBASE_ENTERPRISE
