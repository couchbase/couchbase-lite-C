//
// ReplicatorP2PTest.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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


#ifdef COUCHBASE_ENTERPRISE     // P2P replication is an EE feature


class ReplicatorP2PTest : public ReplicatorTest {
public:
    Database otherDB;
    CBLURLEndpointListenerConfiguration listenerConfig = {};
    CBLListenerAuthenticator listenerAuth = {};
    CBLURLEndpointListener* listener = nullptr;

    ReplicatorP2PTest()
    :otherDB(openEmptyDatabaseNamed("other"))
    {
        config.replicatorType = kCBLReplicatorTypePush;
        config.acceptOnlySelfSignedServerCertificate = true;

        listenerConfig.database = otherDB.ref();
        listenerAuth.context = this;
    }

    ~ReplicatorP2PTest() {
        if (listener) {
            CBLURLEndpointListener_Stop(listener);
            CBLURLEndpointListener_Release(listener);
        }
    }

    void startListener() {
        listener = CBLURLEndpointListener_New(&listenerConfig);
        REQUIRE(listener);
        CBLError error;
        bool started = CBLURLEndpointListener_Start(listener, &error);
        if (!started)
            INFO("Error: " << alloc_slice(CBLError_Description(&error)));
        REQUIRE(started);

        FLMutableArray urls = CBLURLEndpointListener_GetURLs(listener);
        REQUIRE(FLArray_Count(urls) > 0);
        slice url = FLValue_AsString(FLArray_Get(urls, 0));
        REQUIRE(url);
        cout << "Listener is at <" << string(url) << ">\n";
        if (listenerConfig.disableTLS)
            CHECK(url.hasPrefix("ws:"));
        else
            CHECK(url.hasPrefix("wss:"));

        config.endpoint = CBLEndpoint_NewWithURL(url);
    }

    CBLReplicatorStatus replicate() override {
        if (!listener)
            startListener();
        _goodPasswords = _badPasswords = 0;
        return ReplicatorTest::replicate();
    }

    static bool passwordCallback(void* context, FLString username, FLString password) {
        auto self = (ReplicatorP2PTest*)context;
        if (slice(username) == "mortimer"_sl && slice(password) == "sdrawkcab"_sl) {
            ++self->_goodPasswords;
            return true;
        } else {
            ++self->_badPasswords;
            return false;
        }
    }

protected:
    int _goodPasswords = 0, _badPasswords = 0;
};




TEST_CASE_METHOD(ReplicatorP2PTest, "P2P Push", "[Replicator][P2P]") {
    bool useTLS = GENERATE(0, 1);
    if (!useTLS) {
        listenerConfig.disableTLS = true;
        config.acceptOnlySelfSignedServerCertificate = false;
    }
    cout << "------ Use TLS = " << useTLS << "\n";

    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);

    replicate();

    CHECK(asVector(docsNotified) == vector<string>{"foo"});

    Document copiedDoc = otherDB.getDocument("foo");
    REQUIRE(copiedDoc);
    CHECK(copiedDoc["greeting"].asString() == "Howdy!"_sl);
}


TEST_CASE_METHOD(ReplicatorP2PTest, "P2P Password Missing", "[Replicator][P2P]") {
    listenerAuth.passwordAuthenticator = passwordCallback;
    listenerConfig.authenticator = &listenerAuth;

    CBLReplicatorStatus status = replicate();
    CHECK(status.error.domain == CBLWebSocketDomain);
    CHECK(status.error.code == 401);
    CHECK(_badPasswords == 0);
    CHECK(_goodPasswords == 0);
}


TEST_CASE_METHOD(ReplicatorP2PTest, "P2P Password Wrong", "[Replicator][P2P]") {
    listenerAuth.passwordAuthenticator = passwordCallback;
    listenerConfig.authenticator = &listenerAuth;

    config.authenticator = CBLAuth_NewPassword("admin"_sl, "123456"_sl);

    CBLReplicatorStatus status = replicate();
    CHECK(status.error.domain == CBLWebSocketDomain);
    CHECK(status.error.code == 401);
    CHECK(_badPasswords > 0);
    CHECK(_goodPasswords == 0);
}


TEST_CASE_METHOD(ReplicatorP2PTest, "P2P Password Success", "[Replicator][P2P]") {
    listenerAuth.passwordAuthenticator = passwordCallback;
    listenerConfig.authenticator = &listenerAuth;

    config.authenticator = CBLAuth_NewPassword("mortimer"_sl, "sdrawkcab"_sl);

    CBLReplicatorStatus status = replicate();
    CHECK(status.error.code == 0);
    CHECK(_goodPasswords > 0);
}


TEST_CASE_METHOD(ReplicatorP2PTest, "P2P Read-Only", "[Replicator][P2P]") {
    listenerConfig.readOnly = true;

    MutableDocument doc("foo");
    doc["greeting"] = "Howdy!";
    db.saveDocument(doc);

    CBLReplicatorStatus status = replicate();
    CHECK(status.error.domain == CBLWebSocketDomain);
    CHECK(status.error.code == 403);
}


#endif // COUCHBASE_ENTERPRISE
