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

#include "ReplicatorTest.hh"
#include "CBLPrivate.h"
#include "fleece/Fleece.hh"
#include <string>

#ifdef COUCHBASE_ENTERPRISE

static const string kDefaultDocContent = "{\"greeting\":\"hello\"}";

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

    Database db2;
    vector<CBLCollection*> cx;
    vector<CBLCollection*> cy;
};

TEST_CASE_METHOD(URLEndpointListenerTest, "Listener Basics", "[URLListener]") {
    CBLError error {};

    createNumberedDocsWithPrefix(cy[0], 20, "doc2");
    createNumberedDocsWithPrefix(cy[1], 20, "doc2");

    CBLURLEndpointListenerConfiguration listenerConfig {
        .collections = cy.data(),
        .collectionCount = 2,
        .port = 0,
        .disableTLS = true
    };

    CBLURLEndpointListener* listener = nullptr;
    SECTION("0 Collections") {
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
        CBLURLEndpointListener_Free(listener);
    }
}

TEST_CASE_METHOD(URLEndpointListenerTest, "Listener with OneShot Replication", "[URLListener]") {
    CBLError error {};

    CBLURLEndpointListenerConfiguration listenerConfig {
        .collections = cy.data(),
        .collectionCount = 2,
        .port = 0,
        .disableTLS = true
    };

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
    CBLURLEndpointListener_Free(listener);
}

#endif //#ifdef COUCHBASE_ENTERPRISE
