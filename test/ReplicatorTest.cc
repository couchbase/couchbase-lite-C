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
        
        config.endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg:9999/foobar"_sl, &error);
        CHECK(config.endpoint);
        
        proxy.type = kCBLProxyHTTP;
        config.proxy = &proxy;
        CHECK(!CBLReplicator_Create(&config, &error));
    }
    proxy.hostname = "localhost"_sl;
    proxy.port = 9998;
    repl = CBLReplicator_Create(&config, &error);
    CHECK(CBLReplicator_Config(repl) != nullptr);
}


TEST_CASE_METHOD(ReplicatorTest, "Bad url", "[Replicator]") {
    ExpectingExceptions x;
    
    // No db:
    CBLError error;
    auto endpoint = CBLEndpoint_CreateWithURL("ws://localhost:4984"_sl, &error);
    CHECK(!endpoint);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    
    // Invalid scheme:
    endpoint = CBLEndpoint_CreateWithURL("https://localhost:4984/db"_sl, &error);
    CHECK(!endpoint);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
}

#ifndef __ANDROID__
// On Android emulator, the error returned is kCBLNetErrDNSFailure which is a transient error.
TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate", "[Replicator]") {
    CBLError error;
    config.endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl, &error);
    CHECK(config.endpoint);
    
    config.authenticator = CBLAuth_CreateSession("SyncGatewaySession"_sl, "NOM_NOM_NOM"_sl);

    config.pullFilter = [](void *context, CBLDocument* document, CBLDocumentFlags flags) -> bool {
        return true;
    };

    config.pushFilter = [](void *context, CBLDocument* document, CBLDocumentFlags flags) -> bool {
        return true;
    };

    expectedError = {kCBLNetworkDomain, kCBLNetErrUnknownHost};
    replicate();
}
#endif

#ifndef __ANDROID__
// On Android emulator, the error returned is kCBLNetErrDNSFailure which is a transient error.
TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate with auth and proxy", "[Replicator]") {
    CBLError error;
    config.endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl, &error);
    CHECK(config.endpoint);
    
    config.authenticator = CBLAuth_CreatePassword("username"_sl, "p@ssw0RD"_sl);

    CBLProxySettings proxy = {};
    proxy.type = kCBLProxyHTTP;
    proxy.hostname = "jxnbgotn.dvmwk"_sl;
    proxy.port = 9998;
    proxy.username = "User Name"_sl;
    proxy.password = "123456"_sl;
    config.proxy = &proxy;
    
    expectedError = {kCBLNetworkDomain, kCBLNetErrUnknownHost};
    replicate();
}
#endif

#ifndef __ANDROID__
// On Android emulator, the error returned is kCBLNetErrDNSFailure which is a transient error.
// CBL-2337
TEST_CASE_METHOD(ReplicatorTest, "Fake Replicate with freed auth and doc listener", "[Replicator]") {
    CBLError error;
    config.endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl, &error);
    
    CBLAuthenticator* auth = CBLAuth_CreatePassword("username"_sl, "p@ssw0RD"_sl);
    config.authenticator = auth;
    
    repl = CBLReplicator_Create(&config, &error);
    
    // Free the authenticator after creating the replicator
    CBLAuth_Free(auth);
    
    // Note: replicate() will add a document listener
    expectedError = {kCBLNetworkDomain, kCBLNetErrUnknownHost};
    replicate();
    
    // Clean up
    config.authenticator = nullptr;
}
#endif

TEST_CASE_METHOD(ReplicatorTest, "Copy pointer configs", "[Replicator]") {
    CBLReplicatorConfiguration config = {};
    config.database = db.ref();
    
    CBLError error;
    CBLEndpoint* endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl, &error);
    config.endpoint = endpoint;
    
    CBLAuthenticator* auth = nullptr;
    SECTION("Password Auth") {
        auth = CBLAuth_CreatePassword("username"_sl, "p@ssw0RD"_sl);
    }
    SECTION("Session Auth") {
        auth = CBLAuth_CreateSession("abc123"_sl, "mycookie"_sl);
    }
    config.authenticator = auth;
    
    CBLProxySettings* proxy = new CBLProxySettings();
    proxy->type = kCBLProxyHTTP;
    proxy->hostname = "jxnbgotn.dvmwk"_sl;
    proxy->port = 9998;
    proxy->username = "User Name"_sl;
    proxy->password = "123456"_sl;
    config.proxy = proxy;
    
    auto headers = FLMutableDict_New();
    FLMutableDict_SetString(headers, "sessionid"_sl, "abc"_sl);
    config.headers = headers;
    
    auto repl1 = CBLReplicator_Create(&config, &error);
    REQUIRE(repl1);
    
    CBLEndpoint_Free(endpoint);
    CBLAuth_Free(auth);
    delete(proxy);
    FLMutableDict_Release(headers);
    
    auto copiedConfig = CBLReplicator_Config(repl1);
    REQUIRE(copiedConfig);
    CHECK(copiedConfig->endpoint);
    CHECK(copiedConfig->authenticator);
    
    REQUIRE(copiedConfig->proxy);
    CHECK(copiedConfig->proxy->type == kCBLProxyHTTP);
    CHECK(copiedConfig->proxy->hostname == "jxnbgotn.dvmwk"_sl);
    CHECK(copiedConfig->proxy->port == 9998);
    CHECK(copiedConfig->proxy->username == "User Name"_sl);
    CHECK(copiedConfig->proxy->password == "123456"_sl);
    
    REQUIRE(copiedConfig->headers);
    FLValue sessionid = FLDict_Get(copiedConfig->headers, "sessionid"_sl);
    REQUIRE(sessionid);
    CHECK(FLValue_AsString(sessionid) == "abc"_sl);

    auto repl2 = CBLReplicator_Create(copiedConfig, &error);
    CHECK(repl2);
    
    CBLReplicator_Release(repl1);
    CBLReplicator_Release(repl2);
}


TEST_CASE_METHOD(ReplicatorTest, "Copy pointer configs with nullptr value", "[Replicator]") {
    CBLReplicatorConfiguration config = {};
    config.database = db.ref();
    
    CBLError error;
    CBLEndpoint* endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl, &error);
    config.endpoint = endpoint;
    
    auto repl1 = CBLReplicator_Create(&config, &error);
    REQUIRE(repl1);
    
    auto copiedConfig = CBLReplicator_Config(repl1);
    REQUIRE(copiedConfig);
    CHECK(copiedConfig->endpoint);
    CHECK(!copiedConfig->authenticator);
    CHECK(!copiedConfig->proxy);
    CHECK(!copiedConfig->headers);
    
    auto repl2 = CBLReplicator_Create(copiedConfig, &error);
    CHECK(repl2);
    
    CBLReplicator_Release(repl1);
    CBLReplicator_Release(repl2);
}

TEST_CASE_METHOD(ReplicatorTest, "Check userAgent header", "[Replicator]") {
    CBLReplicatorConfiguration config = {};
    config.database = db.ref();

    CBLError error;
    CBLEndpoint* endpoint = CBLEndpoint_CreateWithURL("ws://fsdfds.vzcsg/foobar"_sl, &error);
    config.endpoint = endpoint;

    auto repl1 = CBLReplicator_Create(&config, &error);
    REQUIRE(repl1);

    auto userAgent = slice(CBLReplicator_UserAgent(repl1)).asString();
    CHECK_THAT(userAgent, Catch::Matchers::StartsWith("CouchbaseLite/"));
    CBLReplicator_Release(repl1);
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
            CBL_Log(kCBLLogDomainReplicator, kCBLLogWarning,
                    "Skipping test; server URL not configured");
            return false;
        }
        
        CBLError error;
        config.endpoint = CBLEndpoint_CreateWithURL(slice(serverURL + "/" + dbName), &error);
        CHECK(config.endpoint);
        return true;
    }

    bool setConfigRemoteDBNameTLS(const char *dbName) {
        if (tlsServerURL.empty()) {
            CBL_Log(kCBLLogDomainReplicator, kCBLLogWarning,
                    "Skipping test; server URL not configured");
            return false;
        }
        
        CBLError error;
        config.endpoint = CBLEndpoint_CreateWithURL(slice(tlsServerURL + "/" + dbName), &error);
        CHECK(config.endpoint);
        return true;
    }

    string serverURL, tlsServerURL;
};


TEST_CASE_METHOD(ClientServerReplicatorTest, "HTTP auth", "[Replicator][.Server]") {
    if (!setConfigRemoteDBName("seekrit"))
        return;
    config.replicatorType = kCBLReplicatorTypePull;
    CBLAuthenticator* auth = nullptr;
    CBLError expectedError = CBLError{kCBLWebSocketDomain, 401};
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
    CHECK(defaultCollection.count() == 12189);
}


TEST_CASE_METHOD(ClientServerReplicatorTest, "Pull itunes from SG w/TLS", "[Replicator][.Server]") {
    if (!setConfigRemoteDBNameTLS("itunes"))
        return;
    logEveryDocument = false;
    config.replicatorType = kCBLReplicatorTypePull;
    SECTION("Without cert pinning (fails)") {
        replicate();
        CHECK(replError == (CBLError{kCBLNetworkDomain, kCBLNetErrTLSCertUnknownRoot}));
    }
    SECTION("With cert pinning") {
        alloc_slice serverCert = getServerCert();
        config.pinnedServerCertificate = serverCert;
        replicate();
        CHECK(replError.code == 0);
        CHECK(defaultCollection.count() == 12189);
    }
}
