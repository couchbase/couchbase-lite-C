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

using namespace fleece;


#pragma mark - BASIC TESTS:


TEST_CASE_METHOD(ReplicatorTest, "Bad config", "[Replicator]") {
    CBLProxySettings proxy = {};
    CBLError error;
    {
        ExpectingExceptions x;

#ifndef __clang__
        config.database = nullptr;
        CHECK(!CBLReplicator_Create(&config, &error));
#endif
        
        config.database = db.ref();
        CHECK(!CBLReplicator_Create(&config, &error));

        config.endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg:9999/foobar"_sl);
        proxy.type = kCBLProxyHTTP;
        config.proxy = &proxy;
        CHECK(!CBLReplicator_Create(&config, &error));
    }
    proxy.hostname = "localhost"_sl;
    proxy.port = 9998;
    repl = CBLReplicator_Create(&config, &error);
    CHECK(CBLReplicator_Config(repl) != nullptr);
}


TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate", "[Replicator]") {
    config.endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl);
    config.authenticator = CBLAuth_CreateSession("SyncGatewaySession"_sl, "NOM_NOM_NOM"_sl);

    config.pullFilter = [](void *context, CBLDocument* document, CBLDocumentFlags flags) -> bool {
        return true;
    };

    config.pushFilter = [](void *context, CBLDocument* document, CBLDocumentFlags flags) -> bool {
        return true;
    };

    replicate({CBLNetworkDomain, CBLNetErrUnknownHost});
}


TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate with auth and proxy", "[Replicator]") {
    config.endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl);
    config.authenticator = CBLAuth_CreatePassword("username"_sl, "p@ssw0RD"_sl);

    CBLProxySettings proxy = {};
    proxy.type = kCBLProxyHTTP;
    proxy.hostname = "jxnbgotn.dvmwk"_sl;
    proxy.port = 9998;
    proxy.username = "User Name"_sl;
    proxy.password = "123456"_sl;
    config.proxy = &proxy;
    
    replicate({ CBLNetworkDomain, CBLNetErrUnknownHost });
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
        config.endpoint = CBLEndpoint_CreateWithURL(slice(serverURL + "/" + dbName));
        return true;
    }

    bool setConfigRemoteDBNameTLS(const char *dbName) {
        if (tlsServerURL.empty()) {
            CBL_Log(kCBLLogDomainReplicator, CBLLogWarning,
                    "Skipping test; server URL not configured");
            return false;
        }
        config.endpoint = CBLEndpoint_CreateWithURL(slice(tlsServerURL + "/" + dbName));
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
        auth = CBLAuth_CreatePassword("manhog"_sl, "whim"_sl);
    }
    SECTION("Valid credentials") {
        auth = CBLAuth_CreatePassword("pupshaw"_sl, "frank"_sl);
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
