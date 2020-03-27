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

    string expectedLocalRevID = "??", expectedRemoteRevID = "??";

    void testConflict(bool delLocal, bool delRemote, bool delMerged) {
        deleteLocal = delLocal;
        deleteRemote = delRemote;
        deleteMerged = delMerged;

        if (deleteLocal)
            expectedLocalRevID = "";
        if (delRemote)
            expectedRemoteRevID = "";

        config.replicatorType = kCBLReplicatorTypePull;

        // Save the same doc to each db (will have the same revision),
        MutableDocument doc("foo");
        doc["greeting"] = "Howdy!";
        doc = db.saveDocument(doc).mutableCopy();
        if (deleteLocal) {
            doc.deleteDoc();
        } else {
            doc["expletive"] = "Shazbatt!";
            db.saveDocument(doc);
        }

        doc = MutableDocument("foo");
        doc["greeting"] = "Howdy!";
        doc = otherDB.saveDocument(doc).mutableCopy();
        if (deleteRemote) {
            doc.deleteDoc();
        } else {
            doc["expletive"] = "Frak!";
            otherDB.saveDocument(doc);
        }

        config.conflictResolver = [](void *context,
                                     const char *documentID,
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


    const CBLDocument* conflictResolver(const char *documentID,
                                       const CBLDocument *localDocument,
                                       const CBLDocument *remoteDocument)
    {
        CHECK(!resolverCalled);
        resolverCalled = true;

        CHECK(string(documentID) == "foo");
        if (deleteLocal) {
            REQUIRE(!localDocument);
            REQUIRE(expectedLocalRevID.empty());
        } else {
            REQUIRE(localDocument);
            CHECK(string(CBLDocument_ID(localDocument)) == "foo");
            CHECK(string(CBLDocument_RevisionID(localDocument)) == expectedLocalRevID);
            Dict localProps(CBLDocument_Properties(localDocument));
            CHECK(localProps["greeting"].asString() == "Howdy!"_sl);
            CHECK(localProps["expletive"].asString() == "Shazbatt!"_sl);
        }
        if (deleteRemote) {
            REQUIRE(!remoteDocument);
            REQUIRE(expectedRemoteRevID.empty());
        } else {
            REQUIRE(remoteDocument);
            CHECK(string(CBLDocument_ID(remoteDocument)) == "foo");
            CHECK(string(CBLDocument_RevisionID(remoteDocument)) == expectedRemoteRevID);
            Dict remoteProps(CBLDocument_Properties(remoteDocument));
            CHECK(remoteProps["greeting"].asString() == "Howdy!"_sl);
            CHECK(remoteProps["expletive"].asString() == "Frak!"_sl);
        }
        if (deleteMerged) {
            return nullptr;
        } else {
            CBLDocument *merged = CBLDocument_New(documentID);
            MutableDict mergedProps(CBLDocument_MutableProperties(merged));
            mergedProps.set("greeting"_sl, "¡Hola!");
            // do not release `merged`, otherwise it would be freed before returning!
            return merged;
        }
    }
};


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict (custom resolver)",
                 "[Replicator][Conflict]") {
    expectedLocalRevID = "2-5dd11e6a713b8a346b8ff8a9b04a1da97005990e";
    expectedRemoteRevID = "2-35773cca4b1a10025b7b242709c88024fa7c3713";
    testConflict(false, false, false);
}


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict with remote deletion (custom resolver)",
                 "[Replicator][Conflict]") {
    expectedLocalRevID = "2-5dd11e6a713b8a346b8ff8a9b04a1da97005990e";
    testConflict(false, true, false);
}


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict with local deletion (custom resolver)",
                 "[Replicator][Conflict]") {
    expectedRemoteRevID = "2-35773cca4b1a10025b7b242709c88024fa7c3713";
    testConflict(true, false, false);
}


TEST_CASE_METHOD(ReplicatorConflictTest, "Pull conflict deleting merge (custom resolver)",
                 "[Replicator][Conflict]") {
    expectedLocalRevID = "2-5dd11e6a713b8a346b8ff8a9b04a1da97005990e";
    testConflict(false, true, true);
}


#endif // COUCHBASE_ENTERPRISE
