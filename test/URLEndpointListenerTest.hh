//
// URLEndpointListenerTest.hh
//
// Copyright © 2025 Couchbase. All rights reserved.
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

#ifdef COUCHBASE_ENTERPRISE

class URLEndpointListenerTest : public ReplicatorTest {
public:
    URLEndpointListenerTest()
    :db2(openDatabaseNamed("otherdb", true)) // empty
    {
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

        if (db2) {
            db2.close();
            db2 = nullptr;
        }

        std::set<alloc_slice> labelSet;
        for (const auto& label : identityLabelsToDelete) {
            auto res = labelSet.insert(label);
            // Make sure that a test does not generate identical labels.
            printf("Deleteing %s\n", label.asString().c_str());
            REQUIRE(res.second);
#if !defined(__linux__) && !defined(__ANDROID__)
            CHECK(CBLTLSIdentity_DeleteIdentityWithLabel(label, nullptr));
#else // #if !defined(__linux__) && !defined(__ANDROID__)
            assert(false);
#endif
        }
    }

    CBLEndpoint* clientEndpoint(CBLURLEndpointListener* listener, CBLError* outError);
    vector<CBLReplicationCollection> collectionConfigs(vector<CBLCollection*>collections);
    CBLTLSIdentity* createTLSIdentity(bool isServer, bool withExternalKey);

    static string readFile(const char* filename);

    // OneShot Push, OnlySelfSign, using collections cx
    void configOneShotReplicator(CBLURLEndpointListener* listener, std::vector<CBLReplicationCollection>&);

    Database db2;
    vector<CBLCollection*> cx;
    vector<CBLCollection*> cy;
    vector<alloc_slice> identityLabelsToDelete;
};
#endif //#ifdef COUCHBASE_ENTERPRISE
