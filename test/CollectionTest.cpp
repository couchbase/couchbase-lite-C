//
// CollectionTest.cc
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

#include "CBLTest.hh"
#include "CBLPrivate.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"

using namespace fleece;
using namespace std;

class CollectionTest : public CBLTest {
    
public:
    
    void createNumberedDocs(CBLCollection *col, unsigned n, unsigned start = 1) {
        for (unsigned i = 0; i < n; i++) {
            char docID[20];
            sprintf(docID, "doc-%03u", start + i);
            auto doc = CBLDocument_CreateWithID(slice(docID));
            
            MutableDict props = CBLDocument_MutableProperties(doc);
            char content[100];
            sprintf(content, "This is the document #%03u.", start + i);
            FLSlot_SetString(FLMutableDict_Set(props, "content"_sl), slice(content));
            
            CBLError error;
            bool saved = CBLCollection_SaveDocument(col, doc, &error);
            CBLDocument_Release(doc);
            REQUIRE(saved);
        }
    }
    
};

TEST_CASE_METHOD(CollectionTest, "Default Scope and Collection Name Constant", "[Collection]") {
    CHECK(kCBLDefaultCollectionName == "_default"_sl);
    CHECK(kCBLDefaultScopeName == "_default"_sl);
}

TEST_CASE_METHOD(CollectionTest, "Default Collection", "[Collection]") {
    CBLCollection* col = CBLDatabase_DefaultCollection(db);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
}

TEST_CASE_METHOD(CollectionTest, "Default Collection Exists By Default", "[Collection]") {
    CBLCollection* col = CBLDatabase_DefaultCollection(db);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    col = CBLDatabase_Collection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
    
    scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName);
    CHECK(Array(names).toJSONString() == R"(["_default"])");
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Default Scope Exists By Default", "[Collection]") {
    CBLScope* scope = CBLDatabase_DefaultScope(db);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    scope = CBLDatabase_Scope(db, kCBLDefaultScopeName);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    FLMutableArray names = CBLDatabase_ScopeNames(db);
    CHECK(Array(names).toJSONString() == R"(["_default"])");
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Delete Default Collection", "[Collection]") {
    CBLCollection* col = CBLDatabase_DefaultCollection(db);
    REQUIRE(col);
    
    // Add some docs:
    createNumberedDocs(col, 100);
    CHECK(CBLCollection_Count(col) == 100);
    
    // Delete the default collection:
    CBLError error;
    REQUIRE(CBLDatabase_DeleteCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    CHECK(CBLDatabase_DefaultCollection(db) == nullptr);
    CHECK(CBLDatabase_Collection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName) == nullptr);
    
    // Try to recreate the default collection:
    ExpectingExceptions x;
    CHECK(!CBLDatabase_CreateCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
}

TEST_CASE_METHOD(CollectionTest, "Get Default Scope After Delete Default Collection", "[Collection]") {
    REQUIRE(CBLDatabase_DefaultCollection(db));
    
    // Delete the default collection:
    CBLError error;
    REQUIRE(CBLDatabase_DeleteCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    
    CBLScope* scope = CBLDatabase_DefaultScope(db);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName);
    CHECK(FLArray_Count(names) == 0);
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Upgrade from Database v2.8", "[Collection]") {
    // TODO:
}

TEST_CASE_METHOD(CollectionTest, "Upgrade from Database v3.0", "[Collection]") {
    // TODO:
}

TEST_CASE_METHOD(CollectionTest, "Create And Get Collection In Default Scope", "[Collection]") {
    CBLError error;
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    col = CBLDatabase_Collection(db, "colA"_sl, kCBLDefaultScopeName);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName);
    CHECK(Array(names).toJSONString() == R"(["_default","colA"])");
    FLMutableArray_Release(names);
    
    // Using null scope for the default scope:
    col = CBLDatabase_CreateCollection(db, "colB"_sl, kFLSliceNull, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colB"_sl);
    
    scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    col = CBLDatabase_Collection(db, "colA"_sl, kCBLDefaultScopeName);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName);
    CHECK(Array(names).toJSONString() == R"(["_default","colA","colB"])");
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Create And Get Collection In Named Scope", "[Collection]") {
    CBLError error;
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, "scopeA"_sl);
    CHECK(Array(names).toJSONString() == R"(["colA"])");
    FLMutableArray_Release(names);
    
    // Check the scope and scope names from database:
    scope = CBLDatabase_Scope(db, "scopeA"_sl);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    
    FLMutableArray scopeNames = CBLDatabase_ScopeNames(db);
    CHECK(Array(scopeNames).toJSONString() == R"(["_default","scopeA"])");
    FLMutableArray_Release(scopeNames);
}

TEST_CASE_METHOD(CollectionTest, "Create Existing Collection", "[Collection]") {
    CBLError error;
    CBLCollection* col1 = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1);
    CHECK(CBLCollection_Name(col1) == "colA"_sl);
    
    CBLCollection* col2 = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col2);
    CHECK(CBLCollection_Name(col2) == "colA"_sl);
    
    CHECK(col1 == col2);
}

TEST_CASE_METHOD(CollectionTest, "Get Non Existing Collection", "[Collection]") {
    CBLCollection* col1 = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl);
    CHECK(col1 == nullptr);
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection", "[Collection][.CBL-3142]") {
    CBLError error;
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    // Add some docs:
    createNumberedDocs(col, 100);
    CHECK(CBLCollection_Count(col) == 100);
    
    // Delete:
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl);
    CHECK(col == nullptr);
    
    // Recreate: CBL-3142
    col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CHECK(CBLCollection_Count(col) == 0);

    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CHECK(CBLCollection_Count(col) == 0);
}

