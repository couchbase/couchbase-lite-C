//
// URLEndpointListenerTest.cc
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

#include "CBLTest.hh"
#include "ReplicatorTest.hh"
#include "CBLPrivate.h"
#include "TLSIdentityTest.hh"
#include "fleece/Fleece.hh"
#include <chrono>
#include <string>
#include <sstream>
using namespace std::chrono;

#ifdef COUCHBASE_ENTERPRISE

static const string kDefaultDocContent = "{\"greeting\":\"hello\"}";

namespace {
#ifdef __APPLE__
TLSIdentityTest::ExternalKey* externalKey(void* context) {
    return (TLSIdentityTest::ExternalKey*)context;
}

bool kc_publicKeyData(void* context, void* output, size_t outputMaxLen, size_t* outputLen) {
    return externalKey(context)->publicKeyData(output, outputMaxLen, outputLen);
}

bool kc_decrypt(void* context, FLSlice input, void* output, size_t outputMaxLen, size_t* outputLen) {
    return externalKey(context)->decrypt(input, output, outputMaxLen, outputLen);
}

bool kc_sign(void* context, CBLSignatureDigestAlgorithm digestAlgorithm, FLSlice inputData, void* outSignature) {
    return externalKey(context)->sign(digestAlgorithm, inputData, outSignature);
}

void kc_free(void* context) {
    delete externalKey(context);
}
#endif
}; // anonymous namespace

class URLEndpointListenerTest : public ReplicatorTest {
public:
    URLEndpointListenerTest()
    :db2(openDatabaseNamed("otherdb", true)) // empty
    {
        config.database = nullptr;

        cx.push_back(CreateCollection(db.ref(), "colA", "scopeA"));
        cx.push_back(CreateCollection(db.ref(), "colB", "scopeA"));
        cx.push_back(CreateCollection(db.ref(), "colC", "scopeA"));

        cy.push_back(CreateCollection(db2.ref(), "colA", "scopeA"));
        cy.push_back(CreateCollection(db2.ref(), "colB", "scopeA"));
        cy.push_back(CreateCollection(db2.ref(), "colC", "scopeA"));
    }

    ~URLEndpointListenerTest() {
        for (auto col : cx) {
            CBLCollection_Release(col);
        }
        
        for (auto col : cy) {
            CBLCollection_Release(col);
        }
        
        db2.close();
        db2 = nullptr;
    }

    CBLEndpoint* clientEndpoint(CBLURLEndpointListener* listener, CBLError* outError) {
        uint16_t port = CBLURLEndpointListener_Port(listener);
        const CBLURLEndpointListenerConfiguration* config = CBLURLEndpointListener_Config(listener);
        std::stringstream ss;
        ss << (config->disableTLS ? "ws" : "wss");
        ss << "://localhost:" << port << "/";
        auto listenerConfig = CBLURLEndpointListener_Config(listener);
        REQUIRE(listenerConfig->collectionCount > 0);
        auto db = CBLCollection_Database(listenerConfig->collections[0]);
        REQUIRE(db);
        auto dbname = CBLDatabase_Name(db);
        ss << string(dbname);
        return CBLEndpoint_CreateWithURL(slice(ss.str().c_str()), outError);
    }

    vector<CBLReplicationCollection> collectionConfigs(vector<CBLCollection*>collections) {
        vector<CBLReplicationCollection> configs(collections.size());
        for (int i = 0; i < collections.size(); i++) {
            configs[i].collection = collections[i];
        }
        return configs;
    }

    CBLTLSIdentity* createTLSIdentity(bool isServer, bool withExternalKey) {
        std::unique_ptr<CBLKeyPair, void(*)(CBLKeyPair*)> keypair{
            nullptr,
            [](CBLKeyPair* k) {
                CBLKeyPair_Release(k);
            }
        };
        if (!withExternalKey) {
            keypair.reset(CBLKeyPair_GenerateRSAKeyPair(fleece::nullslice, nullptr));
        } else {
#ifdef __APPLE__
            keypair.reset(CBLKeyPair_CreateWithCallbacks(TLSIdentityTest::ExternalKey::generateRSA(2048),
                                                         2048,
                                                         CBLKeyPairCallbacks{
                kc_publicKeyData, kc_decrypt, kc_sign, kc_free},
                                                         nullptr));
#else
            return nullptr;
#endif
        }
        if (!keypair) return nullptr;

        fleece::MutableDict attributes = fleece::MutableDict::newDict();
        
        attributes[kCBLCertAttrKeyCommonName] = isServer ? "URLEndpointListener" : "URLEndpointListener_Client";

        static constexpr auto validity = seconds(31536000); // one year
        
        CBLKeyUsages usages = isServer ? kCBLKeyUsagesServerAuth : kCBLKeyUsagesClientAuth;
        
        return  CBLTLSIdentity_CreateIdentityWithKeyPair(usages,
                                                         keypair.get(),
                                                         attributes,
                                                         duration_cast<milliseconds>(validity).count(),
                                                         nullptr);
    }

    Database db2;
    vector<CBLCollection*> cx;
    vector<CBLCollection*> cy;
};

TEST_CASE_METHOD(URLEndpointListenerTest, "Listener Basics", "[URLListener]") {
    CBLError error {};

    createNumberedDocsWithPrefix(cy[0], 20, "doc2");
    createNumberedDocsWithPrefix(cy[1], 20, "doc2");

    CBLURLEndpointListenerConfiguration listenerConfig {
        nullptr, // context
        cy.data(),
        2,
        0
    };
    listenerConfig.disableTLS = true;

    CBLURLEndpointListener* listener = nullptr;
    SECTION("0 Collections") {
        ExpectingExceptions x;
        listenerConfig.collectionCount = 0;
        // Cannot create listener with 0 collections.
        listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
        CHECK( (nullptr == listener && error.code == kCBLErrorInvalidParameter && error.domain == kCBLDomain) );
    }

    SECTION("Comparing the Configuration from the Listener") {
        listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
        REQUIRE(listener);

        auto configFromListener = CBLURLEndpointListener_Config(listener);
        REQUIRE(configFromListener);
        // Listener keeps the config by a copy.
        CHECK(&listenerConfig != configFromListener);
        CHECK(0 == memcmp(configFromListener, &listenerConfig, sizeof(CBLURLEndpointListenerConfiguration)));
    }

    SECTION("Port from the Listener") {
        listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
        REQUIRE(listener);
        // Before successful start, the port from the configuration is retuned
        CHECK( 0 == CBLURLEndpointListener_Port(listener));

        REQUIRE(CBLURLEndpointListener_Start(listener, nullptr));
        // Having started, it returns the port selected by the server.
        CHECK( CBLURLEndpointListener_Port(listener) > 0 );
    }

    SECTION("URLs from Listener") {
        listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
        REQUIRE(listener);

        FLMutableArray array = CBLURLEndpointListener_Urls(listener);
        CHECK(array == nullptr);

        REQUIRE(CBLURLEndpointListener_Start(listener, nullptr));
        array = CBLURLEndpointListener_Urls(listener);
        CHECK(array);
        FLSliceResult json = FLValue_ToJSON((FLValue)array);
        CHECK(json.size > 0);
        CHECK(slice(json).containsBytes("\"ws://"));

        FLSliceResult_Release(json);
        FLMutableArray_Release(array);
    }

    if (listener) {
        CBLURLEndpointListener_Stop(listener);
        CBLURLEndpointListener_Release(listener);
    }
}

TEST_CASE_METHOD(URLEndpointListenerTest, "Listener with OneShot Replication", "[URLListener]") {
    CBLError error {};

    CBLURLEndpointListenerConfiguration listenerConfig {
        nullptr, // context
        cy.data(),
        2,
        0
    };
    listenerConfig.disableTLS = true;

    createNumberedDocsWithPrefix(cx[0], 10, "doc");
    createNumberedDocsWithPrefix(cx[1], 10, "doc");
    createNumberedDocsWithPrefix(cy[0], 20, "doc2");
    createNumberedDocsWithPrefix(cy[1], 20, "doc2");
    auto cols = collectionConfigs({cx[0], cx[1]});
    config.collections = cols.data();
    config.collectionCount = cols.size();

    CBLURLEndpointListener* listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
    REQUIRE(listener);

    CHECK(CBLURLEndpointListener_Start(listener, &error));

    CBLEndpoint* replEndpoint = clientEndpoint(listener, &error);
    REQUIRE(replEndpoint);

    config.endpoint = replEndpoint;
    // the lifetime of replEndpoint is passed to config.
    replEndpoint = nullptr;

    SECTION("PUSH") {
        config.replicatorType = kCBLReplicatorTypePush;
        expectedDocumentCount = 20;
        replicate();
    }

    SECTION("PULL") {
        config.replicatorType = kCBLReplicatorTypePull;
        expectedDocumentCount = 40;
        replicate();
    }

    SECTION("PUSH-PULL") {
        config.replicatorType = kCBLReplicatorTypePushAndPull;
        expectedDocumentCount = 60;
        replicate();
    }

    CBLURLEndpointListener_Stop(listener);
    CBLURLEndpointListener_Release(listener);
}

TEST_CASE_METHOD(URLEndpointListenerTest, "Listener with Basic Authentication", "[URLListener]") {
    struct Context {
        int rand = 6801;
    } context;

    static constexpr slice kUser{"pupshaw"};
    static constexpr slice kPassword{"frank"};

    CBLURLEndpointListenerConfiguration listenerConfig {
        &context, // context
        cy.data(),
        2,
        0         // port
    };
    listenerConfig.disableTLS = true;

    SECTION("Successful Login") {
        listenerConfig.authenticator = CBLListenerAuth_CreatePassword([](void* ctx, FLString usr, FLString psw) {
            auto context = reinterpret_cast<Context*>(ctx);
            CHECK(context-> rand  == 6801);
            return usr == kUser && psw == kPassword;
        });
        expectedDocumentCount = 20;
    }

    SECTION("Wrong User") {
        listenerConfig.authenticator = CBLListenerAuth_CreatePassword([](void* ctx, FLString usr, FLString psw) {
            auto context = reinterpret_cast<Context*>(ctx);
            CHECK(context-> rand  == 6801);
            return usr == "InvalidUser"_sl && psw == kPassword;
        });
        expectedError.code = 401;
    }

    SECTION("Wrong Password") {
        listenerConfig.authenticator = CBLListenerAuth_CreatePassword([](void* ctx, FLString usr, FLString psw) {
            auto context = reinterpret_cast<Context*>(ctx);
            CHECK(context-> rand  == 6801);
            return usr == kUser && psw == "InvalidPassword"_sl;
        });
        expectedError.code = 401;
    }

    REQUIRE(listenerConfig.authenticator);

    createNumberedDocsWithPrefix(cx[0], 10, "doc");
    createNumberedDocsWithPrefix(cx[1], 10, "doc");
    createNumberedDocsWithPrefix(cy[0], 20, "doc2");
    createNumberedDocsWithPrefix(cy[1], 20, "doc2");
    auto cols = collectionConfigs({cx[0], cx[1]});
    config.collections = cols.data();
    config.collectionCount = cols.size();

    CBLError error {};
    CBLURLEndpointListener* listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
    REQUIRE(listener);

    CHECK(CBLURLEndpointListener_Start(listener, &error));

    config.endpoint = clientEndpoint(listener, &error);
    REQUIRE(config.endpoint);
    config.authenticator = CBLAuth_CreatePassword(kUser, kPassword);
    REQUIRE(config.authenticator);
    config.replicatorType = kCBLReplicatorTypePush;
    replicate();

    CBLURLEndpointListener_Stop(listener);
    CBLURLEndpointListener_Release(listener);
    if (listenerConfig.authenticator) CBLListenerAuth_Free(listenerConfig.authenticator);
}

TEST_CASE_METHOD(URLEndpointListenerTest, "Listener with Cert Authentication", "[URLListener]") {
    constexpr bool withExternalKey = false;

    struct Context {
        int rand = 6801;
    } context;

    CBLURLEndpointListenerConfiguration listenerConfig {
        &context, // context
        cy.data(),
        2,
        0         // port
    };
    listenerConfig.disableTLS = false;
#if !defined(__linux__) && !defined(__ANDROID__)
    bool anonymous = false;
    alloc_slice persistentLabel;
#endif

    SECTION("Self-signed Cert") {
        listenerConfig.tlsIdentity = createTLSIdentity(true, withExternalKey);
    }

    SECTION("Self-signed Anonymous Cert") {
#if !defined(__linux__) && !defined(__ANDROID__)
        anonymous = true;
        // cy is the collection used in the Listener.
        // Its UUID is used as the label for persistent TLSIdentity
        alloc_slice uuid = CBLDatabase_PublicUUID(CBLCollection_Database(cy[0]));
        persistentLabel = alloc_slice{uuid.hexString()};
        CBLTLSIdentity* id = CBLTLSIdentity_IdentityWithLabel(persistentLabel, nullptr);
        REQUIRE(id == nullptr);
#endif
    }

    listenerConfig.authenticator = CBLListenerAuth_CreateCertificate([](void* ctx, FLSlice certData) {
        auto context = reinterpret_cast<Context*>(ctx);
        CHECK(context->rand  == 6801);
        CBLError error;
        CBLCert* cert = CBLCert_CreateWithData(certData, &error);
        if (!cert) {
            CBL_Log(kCBLLogDomainReplicator, kCBLLogError,
                    "CBLCert_CertFromData failed with code %d", error.code);
            return false;
        }
        alloc_slice sname = CBLCert_SubjectName(cert);
        CBLCert_Release(cert);
        return sname == slice("CN=URLEndpointListener_Client");
    });
    config.acceptOnlySelfSignedServerCertificate = true;
    expectedDocumentCount = 20;

    REQUIRE(listenerConfig.authenticator);
    
    createNumberedDocsWithPrefix(cx[0], 10, "doc");
    createNumberedDocsWithPrefix(cx[1], 10, "doc");
    createNumberedDocsWithPrefix(cy[0], 20, "doc2");
    createNumberedDocsWithPrefix(cy[1], 20, "doc2");
    auto cols = collectionConfigs({cx[0], cx[1]});
    config.collections = cols.data();
    config.collectionCount = cols.size();
    CBLError error {};
    CBLURLEndpointListener* listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
    REQUIRE(listener);
    
    CHECK(CBLURLEndpointListener_Start(listener, &error));

    config.endpoint = clientEndpoint(listener, &error);
    REQUIRE(config.endpoint);
    
    CBLTLSIdentity* clientIdentity = createTLSIdentity(false, withExternalKey);
    REQUIRE(clientIdentity);
    config.authenticator = CBLAuth_CreateCertificate(clientIdentity);
    REQUIRE(config.authenticator);
    config.replicatorType = kCBLReplicatorTypePush;
    replicate();
    
#if !defined(__linux__) && !defined(__ANDROID__)
    if (anonymous) {
        REQUIRE(!!persistentLabel);
        CBLTLSIdentity* id = CBLTLSIdentity_IdentityWithLabel(persistentLabel, nullptr);
        CHECK(id);
        CBLTLSIdentity_Release(id);
        CHECK(CBLTLSIdentity_DeleteIdentityWithLabel(persistentLabel, nullptr));
    }
#endif

    CBLURLEndpointListener_Stop(listener);
    CBLURLEndpointListener_Release(listener);
    CBLTLSIdentity_Release(clientIdentity);
    if (listenerConfig.authenticator) CBLListenerAuth_Free(listenerConfig.authenticator);
    if (listenerConfig.tlsIdentity)
        CBLTLSIdentity_Release(listenerConfig.tlsIdentity);
}

#ifdef __APPLE__
TEST_CASE_METHOD(URLEndpointListenerTest, "Listener with Cert Authentication with External KeyPair", "[URLListener]") {

    struct Context {
        int rand = 6801;
    } context;
    
    CBLURLEndpointListenerConfiguration listenerConfig {
        &context, // context
        cy.data(),
        2,
        0         // port
    };
    listenerConfig.disableTLS = false;

    CBLTLSIdentity* clientIdentity = nullptr;

    SECTION("Server External KeyPair") {
        listenerConfig.tlsIdentity = createTLSIdentity(true, true);
        clientIdentity = createTLSIdentity(false, false);
    }

    SECTION("Client External KeyPair") {
        listenerConfig.tlsIdentity = createTLSIdentity(true, false);
        clientIdentity = createTLSIdentity(false, true);
    }

    SECTION("Server & Client External KeyPairs") {
        listenerConfig.tlsIdentity = createTLSIdentity(true, true);
        clientIdentity = createTLSIdentity(false, true);
    }

    REQUIRE(listenerConfig.tlsIdentity);
    REQUIRE(clientIdentity);

    listenerConfig.authenticator = CBLListenerAuth_CreateCertificate([](void* ctx, FLSlice certData) {
        auto context = reinterpret_cast<Context*>(ctx);
        CHECK(context->rand  == 6801);
        CBLError error;
        CBLCert* cert = CBLCert_CreateWithData(certData, &error);
        if (!cert) {
            CBL_Log(kCBLLogDomainReplicator, kCBLLogError,
                    "CBLCert_CertFromData failed with code %d", error.code);
            return false;
        }
        alloc_slice sname = CBLCert_SubjectName(cert);
        CBLCert_Release(cert);
        return sname == slice("CN=URLEndpointListener_Client");
    });
    config.acceptOnlySelfSignedServerCertificate = true;
    expectedDocumentCount = 20;

    REQUIRE(listenerConfig.authenticator);
    
    createNumberedDocsWithPrefix(cx[0], 10, "doc");
    createNumberedDocsWithPrefix(cx[1], 10, "doc");
    createNumberedDocsWithPrefix(cy[0], 20, "doc2");
    createNumberedDocsWithPrefix(cy[1], 20, "doc2");
    auto cols = collectionConfigs({cx[0], cx[1]});
    config.collections = cols.data();
    config.collectionCount = cols.size();
    CBLError error {};
    CBLURLEndpointListener* listener = CBLURLEndpointListener_Create(&listenerConfig, &error);
    REQUIRE(listener);

    CHECK(CBLURLEndpointListener_Start(listener, &error));

    config.endpoint = clientEndpoint(listener, &error);
    REQUIRE(config.endpoint);

    config.authenticator = CBLAuth_CreateCertificate(clientIdentity);
    REQUIRE(config.authenticator);
    config.replicatorType = kCBLReplicatorTypePush;
    replicate();

    CBLURLEndpointListener_Stop(listener);
    CBLURLEndpointListener_Release(listener);
    CBLTLSIdentity_Release(clientIdentity);
    CBLListenerAuth_Free(listenerConfig.authenticator);
    CBLTLSIdentity_Release(listenerConfig.tlsIdentity);
}
#endif // #ifdef __APPLE__

#endif //#ifdef COUCHBASE_ENTERPRISE
