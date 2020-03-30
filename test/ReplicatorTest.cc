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

#include "ReplicatorTest.hh"


#pragma mark - BASIC TESTS:


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


#pragma mark - ACTUAL-NETWORK TESTS:


/*  The following tests require a running Sync Gateway with a specific set of databases.
    The config files and Walrus database files can be found in the LiteCore repo, at
    vendor/couchbase-lite-core/Replicator/tests/data/

    From a shell in that directory, run `sync_gateway config.json` to start a non-TLS
    server on port 4984, and in another shell run `sync_gateway ssl_config.json` to start
    a TLS server on port 4994.

    When running these tests, set environment variables giving the URLs of the two SG
    instances, e.g:
        CBL_TEST_SERVER_URL=ws://localhost:4984
        CBL_TEST_SERVER_URL_TLS=wss://localhost:4994

    If either variable is not set, the corresponding test(s) will be skipped with a warning.
*/


class ClientServerReplicatorTest : public ReplicatorTest {
public:
    ClientServerReplicatorTest() {
        const char *url = getenv("CBL_TEST_SERVER_URL");
        if (url)
            serverURL = url;
        url = getenv("CBL_TEST_SERVER_URL_TLS");            // e.g. "wss://localhost:4994"
        if (url)
            tlsServerURL = url;
    }

    bool setConfigRemoteDBName(const char *dbName) {
        if (serverURL.empty()) {
            CBL_Log(kCBLLogDomainReplicator, CBLLogWarning,
                    "Skipping test; server URL not configured");
            return false;
        }
        config.endpoint = CBLEndpoint_NewWithURL((serverURL + "/" + dbName).c_str());
        return true;
    }

    bool setConfigRemoteDBNameTLS(const char *dbName) {
        if (tlsServerURL.empty()) {
            CBL_Log(kCBLLogDomainReplicator, CBLLogWarning,
                    "Skipping test; server URL not configured");
            return false;
        }
        config.endpoint = CBLEndpoint_NewWithURL((tlsServerURL + "/" + dbName).c_str());
        return true;
    }

    string serverURL, tlsServerURL;
};


TEST_CASE_METHOD(ClientServerReplicatorTest, "HTTP auth", "[Replicator][.Server]") {
    if (!setConfigRemoteDBName("seekrit"))
        return;
    config.replicatorType = kCBLReplicatorTypePull;
    CBLAuthenticator* auth = nullptr;
    CBLError expectedError = CBLError{CBLWebSocketDomain, 401};
    SECTION("No credentials") {
        auth = nullptr;
    }
    SECTION("Invalid credentials") {
        auth = CBLAuth_NewBasic("manhog", "whim");
    }
    SECTION("Valid credentials") {
        auth = CBLAuth_NewBasic("pupshaw", "frank");
        expectedError = {};
    }
    config.authenticator = auth;
    replicate();
    CHECK(replError == expectedError);
}


TEST_CASE_METHOD(ClientServerReplicatorTest, "Pull itunes from SG", "[Replicator][.Server]") {
    if (!setConfigRemoteDBName("itunes"))
        return;
    logEveryDocument = false;
    config.replicatorType = kCBLReplicatorTypePull;
    replicate();
    CHECK(replError.code == 0);
    CHECK(db.count() == 12189);
}


TEST_CASE_METHOD(ClientServerReplicatorTest, "Pull itunes from SG w/TLS", "[Replicator][.Server]") {
    if (!setConfigRemoteDBNameTLS("itunes"))
        return;
    logEveryDocument = false;
    config.replicatorType = kCBLReplicatorTypePull;
    SECTION("Without cert pinning (fails)") {
        replicate();
        CHECK(replError == (CBLError{CBLNetworkDomain, CBLNetErrTLSCertUnknownRoot}));
    }
    SECTION("With cert pinning") {
        alloc_slice serverCert = getServerCert();
        config.pinnedServerCertificate = serverCert;
        replicate();
        CHECK(replError.code == 0);
        CHECK(db.count() == 12189);
    }
}
