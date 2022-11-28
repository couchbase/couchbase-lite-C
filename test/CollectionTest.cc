//
// CollectionTest.cc
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
#include "CBLPrivate.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"

using namespace fleece;
using namespace std;

static constexpr size_t kDocIDBufferSize = 20;

class CollectionTest : public CBLTest {
    
public:
    
    void createNumberedDocs(CBLCollection *col, unsigned n, unsigned start = 1) {
        for (unsigned i = 0; i < n; i++) {
            char docID[kDocIDBufferSize];
            snprintf(docID, kDocIDBufferSize, "doc-%03u", start + i);
            auto doc = CBLDocument_CreateWithID(slice(docID));
            
            MutableDict props = CBLDocument_MutableProperties(doc);
            char content[100];
            snprintf(content, 100, "This is the document #%03u.", start + i);
            FLSlot_SetString(FLMutableDict_Set(props, "content"_sl), slice(content));
            
            CBLError error = {};
            bool saved = CBLCollection_SaveDocument(col, doc, &error);
            CBLDocument_Release(doc);
            REQUIRE(saved);
        }
    }
    
    CBLDatabase* openDB() {
        CBLError error = {};
        auto config = databaseConfig();
        CBLDatabase* db = CBLDatabase_Open(kDatabaseName, &config, &error);
        REQUIRE(db);
        return db;
    }
    
    void testInvalidCollection(CBLCollection* col) {
        REQUIRE(col);
        
        ExpectingExceptions x;
        
        // Properties:
        CHECK(CBLCollection_Name(col));
        CHECK(CBLCollection_Scope(col));
        CHECK(CBLCollection_Count(col) == 0);
        
        // Document Functions:
        CBLError error = {};
        auto doc = CBLDocument_CreateWithID("doc1"_sl);
        CHECK(!CBLCollection_SaveDocument(col, doc, &error));
        CheckNotOpenError(error);
        
        error = {};
        auto conflictHandler = [](void *c, CBLDocument* d1, const CBLDocument* d2) -> bool { return true; };
        CHECK(!CBLCollection_SaveDocumentWithConflictHandler(col, doc, conflictHandler, nullptr, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_SaveDocumentWithConcurrencyControl(col, doc, kCBLConcurrencyControlLastWriteWins, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetDocument(col, "doc1"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetMutableDocument(col, "doc1"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_DeleteDocument(col, doc, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_DeleteDocumentWithConcurrencyControl(col, doc, kCBLConcurrencyControlLastWriteWins, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_PurgeDocument(col, doc, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_PurgeDocumentByID(col, "doc1"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(CBLCollection_GetDocumentExpiration(col, "doc1"_sl, &error) == -1);
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_SetDocumentExpiration(col, "doc1"_sl, CBL_Now(), &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_CreateValueIndex(col, "Value"_sl, {}, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_CreateFullTextIndex(col, "FTS"_sl, {}, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetIndexNames(col, &error));
        CheckNotOpenError(error);
        
        auto listener = [](void* ctx, const CBLCollectionChange* change) { };
        auto token = CBLCollection_AddChangeListener(col, listener, nullptr);
        CBLListener_Remove(token);
        
        auto docListener = [](void* ctx, const CBLDocumentChange* change) { };
        token = CBLCollection_AddDocumentChangeListener(col, "doc1"_sl, docListener, nullptr);
        CBLListener_Remove(token);
        
        // Release:
        CBLDocument_Release(doc);
    }
    
    void testInvalidScope(CBLScope* scope) {
        REQUIRE(scope);
        
        CHECK(CBLScope_Name(scope));
        
        ExpectingExceptions x;
        
        CBLError error = {};
        CHECK(!CBLScope_Collection(scope, "collection"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLScope_CollectionNames(scope, &error));
        CheckNotOpenError(error);
    }
    
    void testInvalidDatabase(CBLDatabase* database) {
        REQUIRE(database);
        
        ExpectingExceptions x;
        
        CBLError error = {};
        CHECK(!CBLDatabase_DefaultScope(db, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_DefaultCollection(db, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_ScopeNames(db, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_CollectionNames(db, "_default"_sl,  &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_Collection(db, "_default"_sl, "_default"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_Scope(db, "_default"_sl, &error));
        CheckNotOpenError(error);
    }
};

TEST_CASE_METHOD(CollectionTest, "Default Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_DefaultCollection(db, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Default Collection Exists By Default", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_DefaultCollection(db, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLCollection_Release(col);
    
    col = CBLDatabase_Collection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
    
    scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLCollection_Release(col);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName, &error);
    CHECK(Array(names).toJSONString() == R"(["_default"])");
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Default Scope Exists By Default", "[Collection]") {
    CBLError error = {};
    CBLScope* scope = CBLDatabase_DefaultScope(db, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLScope_Release(scope);
    
    scope = CBLDatabase_Scope(db, kCBLDefaultScopeName, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLScope_Release(scope);
    
    FLMutableArray names = CBLDatabase_ScopeNames(db, &error);
    CHECK(Array(names).toJSONString() == R"(["_default"])");
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Delete Default Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_DefaultCollection(db, &error);
    REQUIRE(col);
    
    // Add some docs:
    createNumberedDocs(col, 100);
    CHECK(CBLCollection_Count(col) == 100);
    CBLCollection_Release(col);
    
    // Delete the default collection:
    REQUIRE(CBLDatabase_DeleteCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    CHECK(!CBLDatabase_DefaultCollection(db, &error));
    CHECK(error.code == 0);
    CHECK(!CBLDatabase_Collection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    CHECK(error.code == 0);
    
    // Try to recreate the default collection:
    ExpectingExceptions x;
    CHECK(!CBLDatabase_CreateCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
}

TEST_CASE_METHOD(CollectionTest, "Get Default Scope After Delete Default Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_DefaultCollection(db, &error);
    REQUIRE(col);
    CBLCollection_Release(col);
    
    // Delete the default collection:
    REQUIRE(CBLDatabase_DeleteCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    
    CBLScope* scope = CBLDatabase_DefaultScope(db, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLScope_Release(scope);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName, &error);
    CHECK(FLArray_Count(names) == 0);
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Create And Get Collection In Default Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLCollection_Release(col);
    
    col = CBLDatabase_Collection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CBLCollection_Release(col);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName, &error);
    CHECK(Array(names).toJSONString() == R"(["_default","colA"])");
    FLMutableArray_Release(names);
    
    // Using null scope for the default scope:
    col = CBLDatabase_CreateCollection(db, "colB"_sl, kFLSliceNull, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colB"_sl);
    
    scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLCollection_Release(col);
    
    col = CBLDatabase_Collection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CBLCollection_Release(col);
    
    names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName, &error);
    CHECK(Array(names).toJSONString() == R"(["_default","colA","colB"])");
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Create And Get Collection In Named Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    CBLCollection_Release(col);
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CBLCollection_Release(col);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, "scopeA"_sl, &error);
    CHECK(Array(names).toJSONString() == R"(["colA"])");
    FLMutableArray_Release(names);
    
    // Check the scope and scope names from database:
    scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    CBLScope_Release(scope);
    
    FLMutableArray scopeNames = CBLDatabase_ScopeNames(db, &error);
    CHECK(Array(scopeNames).toJSONString() == R"(["_default","scopeA"])");
    FLMutableArray_Release(scopeNames);
}

TEST_CASE_METHOD(CollectionTest, "Create Existing Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1 = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1);
    CHECK(CBLCollection_Name(col1) == "colA"_sl);
    
    CBLCollection* col2 = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col2);
    CHECK(CBLCollection_Name(col2) == "colA"_sl);
    
    CHECK(col1 == col2);
    
    CBLCollection_Release(col1);
    CBLCollection_Release(col2);
}

TEST_CASE_METHOD(CollectionTest, "Get Non Existing Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1 = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    CHECK(col1 == nullptr);
    CHECK(error.code == 0);
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CBLCollection_Release(col);
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    // Add some docs:
    createNumberedDocs(col, 100);
    CHECK(CBLCollection_Count(col) == 100);
    CBLCollection_Release(col);
    
    // Delete:
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    CHECK(col == nullptr);
    CHECK(error.code == 0);
    
    // Recreate:
    col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CHECK(CBLCollection_Count(col) == 0);
    CBLCollection_Release(col);
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CHECK(CBLCollection_Count(col) == 0);
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Get Collections from Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* colA = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(colA);
    
    CBLCollection* colB = CBLDatabase_CreateCollection(db, "colB"_sl, "scopeA"_sl, &error);
    REQUIRE(colB);
    
    CBLScope* scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    
    CBLCollection* colA2 = CBLScope_Collection(scope, "colA"_sl, &error);
    CHECK(colA == colA2);
    
    CBLCollection* colB2 = CBLScope_Collection(scope, "colB"_sl, &error);
    CHECK(colB == colB2);
    
    CHECK(!CBLScope_Collection(scope, "colC"_sl, &error));
    CHECK(error.code == 0);
    
    FLMutableArray colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"(["colA","colB"])");
    FLMutableArray_Release(colNames);
    
    CBLScope_Release(scope);
    CBLCollection_Release(colA);
    CBLCollection_Release(colB);
    CBLCollection_Release(colA2);
    CBLCollection_Release(colB2);
}

TEST_CASE_METHOD(CollectionTest, "Delete All Collections in Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* colA = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(colA);
    
    CBLCollection* colB = CBLDatabase_CreateCollection(db, "colB"_sl, "scopeA"_sl, &error);
    REQUIRE(colB);
    
    CBLScope* scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    FLMutableArray scopeNames = CBLDatabase_ScopeNames(db, &error);
    CHECK(Array(scopeNames).toJSONString() == R"(["_default","scopeA"])");
    FLMutableArray_Release(scopeNames);
    
    // Delete all collections in the scope:
    FLMutableArray colNames = CBLScope_CollectionNames(scope, &error);
    for (Array::iterator i(colNames); i; ++i) {
        REQUIRE(CBLDatabase_DeleteCollection(db, i->asString(), CBLScope_Name(scope), &error));
    }
    FLMutableArray_Release(colNames);
    
    // Get collections from the scope object:
    CHECK(!CBLScope_Collection(scope, "colA"_sl, &error));
    CHECK(error.code == 0);
    
    CHECK(!CBLScope_Collection(scope, "colB"_sl, &error));
    CHECK(error.code == 0);
    
    colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"([])");
    FLMutableArray_Release(colNames);
    
    // Check that the scope doesn't exist:
    CHECK(!CBLDatabase_Scope(db, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    scopeNames = CBLDatabase_ScopeNames(db, &error);
    CHECK(Array(scopeNames).toJSONString() == R"(["_default"])");
    FLMutableArray_Release(scopeNames);
    
    CBLScope_Release(scope);
    CBLCollection_Release(colA);
    CBLCollection_Release(colB);
}

TEST_CASE_METHOD(CollectionTest, "Valid Collection and Scope Names", "[Collection]") {
    vector<string> names {
        "a", "B", "0", "-",
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_%"
    };

    for (auto name : names) {
        CBLError error = {};
        CBLCollection* col1 = CBLDatabase_CreateCollection(db, slice(name), slice(name), &error);
        REQUIRE(col1);
        
        CBLCollection* col2 = CBLDatabase_Collection(db, slice(name), slice(name), &error);
        REQUIRE(col2);
        
        CHECK(col1 == col2);
        
        CBLCollection_Release(col1);
        CBLCollection_Release(col2);
    }
}

TEST_CASE_METHOD(CollectionTest, "Invalid Collection and Scope Names", "[Collection]") {
    vector<string> names = {
        "_a", "%a", /* Invalid Prefix */
    };
    
    // Invalid special characters:
    string specialChars = "!@#$^&*()+={}[]<>,.?/:;\"'\\|`~";
    for (auto &ch : specialChars) {
        names.push_back("a" + string(1, ch) + "z");
    }
    
    for (auto name : names) {
        ExpectingExceptions x;
        CBLError error = {};
        CBLCollection* col = CBLDatabase_CreateCollection(db, slice(name), "scopeA"_sl, &error);
        REQUIRE(!col);
        CHECK(error.domain == kCBLDomain);
        CHECK(error.code == kCBLErrorInvalidParameter);
        CBLCollection_Release(col);
        
        col = CBLDatabase_CreateCollection(db, "colA"_sl, slice(name), &error);
        REQUIRE(!col);
        CHECK(error.domain == kCBLDomain);
        CHECK(error.code == kCBLErrorInvalidParameter);
    }
}

TEST_CASE_METHOD(CollectionTest, "Overflow Collection and Scope Names", "[Collection]") {
    string name = "";
    for (int i = 0; i < 251; i++) {
        name = name + "a";
    }
    
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, slice(name), slice(name), &error);
    REQUIRE(col);
    CBLCollection_Release(col);
    
    ExpectingExceptions x;
    
    name = name + "a";
    col = CBLDatabase_CreateCollection(db, slice(name), "scopeA"_sl, &error);
    REQUIRE(!col);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    
    col = CBLDatabase_CreateCollection(db, "colA"_sl, slice(name), &error);
    REQUIRE(!col);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
}

TEST_CASE_METHOD(CollectionTest, "Collection Name Case Sensitive", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "COL1"_sl, "scopeA"_sl, &error);
    REQUIRE(col1a);
    
    CBLCollection* col1b = CBLDatabase_CreateCollection(db, "col1"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    
    CHECK(col1a != col1b);
    
    FLMutableArray colNames = CBLDatabase_CollectionNames(db, "scopeA"_sl, &error);
    CHECK(Array(colNames).toJSONString() == R"(["COL1","col1"])");
    FLMutableArray_Release(colNames);
    
    CBLCollection_Release(col1a);
    CBLCollection_Release(col1b);
}

TEST_CASE_METHOD(CollectionTest, "Scope Name Case Sensitive", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "col1"_sl, "SCOPEA"_sl, &error);
    REQUIRE(col1a);
    
    CBLCollection* col1b = CBLDatabase_CreateCollection(db, "col1"_sl, "scopea"_sl, &error);
    REQUIRE(col1b);
    
    CHECK(col1a != col1b);
    
    FLMutableArray scopeNames = CBLDatabase_ScopeNames(db, &error);
    CHECK(Array(scopeNames).toJSONString() == R"(["_default","SCOPEA","scopea"])");
    FLMutableArray_Release(scopeNames);
    
    CBLCollection_Release(col1a);
    CBLCollection_Release(col1b);
}

TEST_CASE_METHOD(CollectionTest, "Create then Get Collection using Different DB Instances", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1a);
    createNumberedDocs(col1a, 10);
    CHECK(CBLCollection_Count(col1a) == 10);
    
    // Using another instance to get the collection:
    CBLDatabase* db2 = openDB();
    CBLCollection* col1b = CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    CHECK(col1a != col1b);
    CHECK(CBLCollection_Count(col1b) == 10);
    
    // Create another 10 docs in col1b:
    createNumberedDocs(col1b, 10, 100);
    CHECK(CBLCollection_Count(col1b) == 20);
    CHECK(CBLCollection_Count(col1a) == 20);
    
    CBLDatabase_Release(db2);
    
    CBLCollection_Release(col1a);
    CBLCollection_Release(col1b);
}

TEST_CASE_METHOD(CollectionTest, "Create then Create Collection using Different DB Instances", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1a);
    createNumberedDocs(col1a, 10);
    CHECK(CBLCollection_Count(col1a) == 10);
    
    // Using another instance to create the collection again:
    CBLDatabase* db2 = openDB();
    CBLCollection* col1b = CBLDatabase_CreateCollection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    CHECK(col1a != col1b);
    CHECK(CBLCollection_Count(col1b) == 10);
    
    // Create another 10 docs in col1b:
    createNumberedDocs(col1b, 10, 100);
    CHECK(CBLCollection_Count(col1b) == 20);
    CHECK(CBLCollection_Count(col1a) == 20);
    
    CBLCollection_Release(col1a);
    CBLCollection_Release(col1b);
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Delete then Get Collection from Different DB Instances", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1a);
    createNumberedDocs(col1a, 10);
    CHECK(CBLCollection_Count(col1a) == 10);
    
    CBLDatabase* db2 = openDB();
    CBLCollection* col1b = CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    CHECK(col1a != col1b);
    CHECK(CBLCollection_Count(col1b) == 10);
    
    // Delete the collection from db:
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    {
        ExpectingExceptions x;
        CHECK(CBLCollection_Count(col1a) == 0);
        CHECK(CBLCollection_Count(col1b) == 0);
    }
    CHECK(!CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    CHECK(!CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    CBLCollection_Release(col1a);
    CBLCollection_Release(col1b);
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Delete and Recreate then Get Collection from Different DB Instances", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1a);
    createNumberedDocs(col1a, 10);
    CHECK(CBLCollection_Count(col1a) == 10);
    
    CBLDatabase* db2 = openDB();
    CBLCollection* col1b = CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    CHECK(col1a != col1b);
    CHECK(CBLCollection_Count(col1b) == 10);
    
    // Delete the collection from db:
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(!CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(!CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error));
    
    // Recreate:
    CBLCollection* col1c = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1c);
    CHECK(col1c != col1a);
    {
        ExpectingExceptions x;
        CHECK(CBLCollection_Count(col1a) == 0);
        CHECK(CBLCollection_Count(col1b) == 0);
    }
    CHECK(CBLCollection_Count(col1c) == 0);
    
    CBLCollection* col1d = CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1d);
    CHECK(col1d != col1b);
    
    CBLCollection_Release(col1a);
    CBLCollection_Release(col1b);
    CBLCollection_Release(col1c);
    CBLCollection_Release(col1d);
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection then Use Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    
    testInvalidCollection(col);
    
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection from Different DB Instance then Use Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    CBLDatabase* db2 = openDB();
    REQUIRE(CBLDatabase_DeleteCollection(db2, "colA"_sl, "scopeA"_sl, &error));
    
    testInvalidCollection(col);
    
    CBLCollection_Release(col);
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Delete Scope then Use Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    CBLScope* scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    REQUIRE(scope);
    
    FLMutableArray colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"(["colA"])");
    FLMutableArray_Release(colNames);
    
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(!CBLDatabase_Scope(db, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"([])");
    FLMutableArray_Release(colNames);
    
    CHECK(!CBLScope_Collection(scope, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    CBLCollection_Release(col);
    CBLScope_Release(scope);
}

TEST_CASE_METHOD(CollectionTest, "Delete Scope from Different DB Instance then Use Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    CBLScope* scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    REQUIRE(scope);
    
    FLMutableArray colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"(["colA"])");
    FLMutableArray_Release(colNames);
    
    CBLDatabase* db2 = openDB();
    REQUIRE(CBLDatabase_DeleteCollection(db2, "colA"_sl, "scopeA"_sl, &error));
    CHECK(!CBLDatabase_Scope(db, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"([])");
    FLMutableArray_Release(colNames);
    
    CHECK(!CBLScope_Collection(scope, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    CBLCollection_Release(col);
    CBLScope_Release(scope);
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Close Database then Use Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    REQUIRE(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    db = nullptr;
    
    testInvalidCollection(col);
    
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Delete Database then Use Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    REQUIRE(CBLDatabase_Delete(db, &error));
    CBLDatabase_Release(db);
    db = nullptr;
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    
    testInvalidScope(scope);
    
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Close Database then Use Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    REQUIRE(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    db = nullptr;
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    
    testInvalidScope(scope);
    
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Close Database then Create Or Get Collections and Scopes", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CBLCollection_Release(col);
    
    REQUIRE(CBLDatabase_Close(db, &error));
    
    testInvalidDatabase(db);
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection and Close Database then Use Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(!CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    REQUIRE(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    db = nullptr;
    
    testInvalidCollection(col);
    
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Delete Scope and Close Database then Use Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* colA = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(colA);
    CBLCollection_Release(colA);
    
    CBLScope* scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(!CBLDatabase_Scope(db, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    REQUIRE(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    db = nullptr;
    
    testInvalidScope(scope);
    
    CBLScope_Release(scope);
}
