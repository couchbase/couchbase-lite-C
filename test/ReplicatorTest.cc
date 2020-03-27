//
// ReplicatorTest.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "CBLTest.hh"
#include "cbl++/CouchbaseLite.hh"
#include <iostream>
#include <thread>
#include <set>

using namespace std;
using namespace fleece;
using namespace cbl;


class ReplicatorTest : public CBLTest_Cpp {
public:
    CBLReplicatorConfiguration config = {};
    CBLReplicator *repl = nullptr;
    set<string> docsNotified;

    ReplicatorTest() {
        config.database = db.ref();
        config.replicatorType = kCBLReplicatorTypePull;
        config.context = this;
    }

    void replicate() {
        CBLError error;
        repl = CBLReplicator_New(&config, &error);
        REQUIRE(repl);

        auto ctoken = CBLReplicator_AddChangeListener(repl, [](void *context, CBLReplicator *r,
                                                 const CBLReplicatorStatus *status) {
            ((ReplicatorTest*)context)->statusChanged(r, *status);
        }, this);

        auto dtoken = CBLReplicator_AddDocumentListener(repl, [](void *context, CBLReplicator *r, bool isPush,
                                                                 unsigned numDocuments,
                                                                 const CBLReplicatedDocument* documents) {
            ((ReplicatorTest*)context)->docProgress(r, isPush, numDocuments, documents);
        }, this);

        CBLReplicator_Start(repl);

        cerr << "Waiting...\n";
        CBLReplicatorStatus status;
        while ((status = CBLReplicator_Status(repl)).activity != kCBLReplicatorStopped) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
        cerr << "Finished with activity=" << status.activity
             << ", error=(" << status.error.domain << "/" << status.error.code << ")\n";

        CBLListener_Remove(ctoken);
        CBLListener_Remove(dtoken);
    }

    void statusChanged(CBLReplicator *r, const CBLReplicatorStatus &status) {
        CHECK(r == repl);
        cerr << "--- PROGRESS: status=" << status.activity << ", fraction=" << status.progress.fractionComplete << ", err=" << status.error.domain << "/" << status.error.code << "\n";
    }

    void docProgress(CBLReplicator *r, bool isPush,
                     unsigned numDocuments,
                     const CBLReplicatedDocument* documents) {
        CHECK(r == repl);
        cerr << "--- " << numDocuments << " docs " << (isPush ? "pushed" : "pulled") << ":";
        for (unsigned i = 0; i < numDocuments; ++i) {
            docsNotified.insert(documents[i].ID);
            cerr << " " << documents[i].ID;
        }
        cerr << "\n";
    }

    ~ReplicatorTest() {
        CBLReplicator_Release(repl);
        CBLAuth_Free(config.authenticator);
        CBLEndpoint_Free(config.endpoint);
    }

#ifdef COUCHBASE_ENTERPRISE
    /// Creates `otherDB` and configures the replication to push to it
    void configureLocalReplication() {
        otherDB = openEmptyDatabaseNamed("otherDB");
        config.endpoint = CBLEndpoint_NewWithLocalDB(otherDB.ref());
        config.replicatorType = kCBLReplicatorTypePush;
    }

    static vector<string> asVector(const set<string> strings) {
        vector<string> out;
        for (const string &s : strings)
            out.push_back(s);
        return out;
    }

    Database otherDB;
#endif // COUCHBASE_ENTERPRISE
};


#pragma mark - TESTS:


TEST_CASE_METHOD(ReplicatorTest, "Bad config", "[Replicator]") {
    CBLError error;
    config.database = nullptr;
    CHECK(!CBLReplicator_New(&config, &error));

    config.database = db.ref();
    CHECK(!CBLReplicator_New(&config, &error));

    config.endpoint = CBLEndpoint_NewWithURL("ws://localhost:9999/foobar");
    CBLProxySettings proxy = {};
    proxy.type = kCBLProxyHTTP;
    config.proxy = &proxy;
    CHECK(!CBLReplicator_New(&config, &error));

    proxy.hostname = "localhost";
    proxy.port = 9998;
    repl = CBLReplicator_New(&config, &error);
    CHECK(repl);
}


TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate", "[Replicator]") {
    config.endpoint = CBLEndpoint_NewWithURL("ws://localhost:9999/foobar");
    config.authenticator = CBLAuth_NewSession("SyncGatewaySession", "NOM_NOM_NOM");

    config.pullFilter = [](void *context, CBLDocument* document, bool isDeleted) -> bool {
        return true;
    };

    config.pushFilter = [](void *context, CBLDocument* document, bool isDeleted) -> bool {
        return true;
    };

    replicate();
}


TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate with auth and proxy", "[Replicator]") {
    config.endpoint = CBLEndpoint_NewWithURL("ws://localhost:9999/foobar");
    config.authenticator = CBLAuth_NewBasic("username", "p@ssw0RD");

    CBLProxySettings proxy = {};
    proxy.type = kCBLProxyHTTP;
    proxy.hostname = "localhost";
    proxy.port = 9998;
    proxy.username = "User Name";
    proxy.password = "123456";
    config.proxy = &proxy;

    replicate();
}


#pragma mark - LOCAL-TO-LOCAL TESTS:


#ifdef COUCHBASE_ENTERPRISE

TEST_CASE_METHOD(ReplicatorTest, "Push to local db", "[Replicator]") {
    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);

    configureLocalReplication();
    replicate();

    CHECK(asVector(docsNotified) == vector<string>{"foo"});

    Document copiedDoc = otherDB.getDocument("foo");
    REQUIRE(copiedDoc);
    CHECK(copiedDoc["greeting"].asString() == "Howdy!"_sl);
}


TEST_CASE_METHOD(ReplicatorTest, "Pull conflict (default resolver)", "[Replicator][Conflict]") {
    configureLocalReplication();
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


class ReplicatorConflictTest : public ReplicatorTest {
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

        configureLocalReplication();
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
