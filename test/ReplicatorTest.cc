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

using namespace std;
using namespace fleece;
using namespace cbl;


class ReplicatorTest : public CBLTest_Cpp {
public:
    CBLReplicatorConfiguration config = {};
    CBLReplicator *repl = nullptr;

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
            ((ReplicatorTest*)context)->docsChanged(r, isPush, numDocuments, documents);
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

    void docsChanged(CBLReplicator *r, bool isPush,
                     unsigned numDocuments,
                     const CBLReplicatedDocument* documents) {
        CHECK(r == repl);
        cerr << "--- " << numDocuments << " docs " << (isPush ? "pushed" : "pulled") << ":";
        for (unsigned i = 0; i < numDocuments; ++i)
            cerr << " " << documents[i].ID;
        cerr << "\n";
    }

    ~ReplicatorTest() {
        CBLReplicator_Release(repl);
        CBLAuth_Free(config.authenticator);
        CBLEndpoint_Free(config.endpoint);
    }
};


#pragma mark - TESTS:


TEST_CASE_METHOD(ReplicatorTest, "Bad config") {
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


TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate") {
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


TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate with auth and proxy") {
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
