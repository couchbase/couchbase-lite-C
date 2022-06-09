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
            
            CBLError error = {};
            bool saved = CBLCollection_SaveDocument(col, doc, &error);
            CBLDocument_Release(doc);
            REQUIRE(saved);
        }
    }
    
    CBLDatabase* openDB() {
        CBLError error = {};
        CBLDatabase* db = CBLDatabase_Open(kDatabaseName, &kDatabaseConfiguration, &error);
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
        checkNotOpenError(error);
        
        error = {};
        auto conflictHandler = [](void *c, CBLDocument* d1, const CBLDocument* d2) -> bool { return true; };
        CHECK(!CBLCollection_SaveDocumentWithConflictHandler(col, doc, conflictHandler, nullptr, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_SaveDocumentWithConcurrencyControl(col, doc, kCBLConcurrencyControlLastWriteWins, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetDocument(col, "doc1"_sl, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetMutableDocument(col, "doc1"_sl, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_DeleteDocument(col, doc, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_DeleteDocumentWithConcurrencyControl(col, doc, kCBLConcurrencyControlLastWriteWins, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_PurgeDocument(col, doc, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_PurgeDocumentByID(col, "doc1"_sl, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(CBLCollection_GetDocumentExpiration(col, "doc1"_sl, &error) == 0);
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_SetDocumentExpiration(col, "doc1"_sl, CBL_Now(), &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_CreateValueIndex(col, "Value"_sl, {}, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_CreateFullTextIndex(col, "FTS"_sl, {}, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLCollection_GetIndexNames(col, &error));
        checkNotOpenError(error);
        
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
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLScope_CollectionNames(scope, &error));
        checkNotOpenError(error);
    }
    
    void testInvalidDatabase(CBLDatabase* database) {
        REQUIRE(database);
        
        ExpectingExceptions x;
        
        CBLError error = {};
        CHECK(!CBLDatabase_DefaultScope(db, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_DefaultCollection(db, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_ScopeNames(db, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_CollectionNames(db, "_default"_sl,  &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_Collection(db, "_default"_sl, "_default"_sl, &error));
        checkNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_Scope(db, "_default"_sl, &error));
        checkNotOpenError(error);
    }
};

TEST_CASE_METHOD(CollectionTest, "Default Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_DefaultCollection(db, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
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
    
    col = CBLDatabase_Collection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
    
    scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, kCBLDefaultScopeName, &error);
    CHECK(Array(names).toJSONString() == R"(["_default"])");
    FLMutableArray_Release(names);
}

TEST_CASE_METHOD(CollectionTest, "Default Scope Exists By Default", "[Collection]") {
    CBLError error = {};
    CBLScope* scope = CBLDatabase_DefaultScope(db, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
    scope = CBLDatabase_Scope(db, kCBLDefaultScopeName, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
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
    REQUIRE(CBLDatabase_DefaultCollection(db, &error));
    
    // Delete the default collection:
    REQUIRE(CBLDatabase_DeleteCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    
    CBLScope* scope = CBLDatabase_DefaultScope(db, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    
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
    
    col = CBLDatabase_Collection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
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
    
    col = CBLDatabase_Collection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
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
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    FLMutableArray names = CBLDatabase_CollectionNames(db, "scopeA"_sl, &error);
    CHECK(Array(names).toJSONString() == R"(["colA"])");
    FLMutableArray_Release(names);
    
    // Check the scope and scope names from database:
    scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    
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
}

TEST_CASE_METHOD(CollectionTest, "Get Non Existing Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1 = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    CHECK(col1 == nullptr);
    CHECK(error.code == 0);
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection", "[.CBL-3142]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    // Add some docs:
    createNumberedDocs(col, 100);
    CHECK(CBLCollection_Count(col) == 100);
    
    // Delete:
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    
    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    CHECK(col == nullptr);
    CHECK(error.code == 0);
    
    // Recreate: CBL-3142
    col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CHECK(CBLCollection_Count(col) == 0);

    col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    CHECK(CBLCollection_Count(col) == 0);
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
    
    CHECK(CBLScope_Collection(scope, "colA"_sl, &error) == colA);
    CHECK(CBLScope_Collection(scope, "colB"_sl, &error) == colB);
    CHECK(!CBLScope_Collection(scope, "colC"_sl, &error));
    CHECK(error.code == 0);
    
    FLMutableArray colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"(["colA","colB"])");
    FLMutableArray_Release(colNames);
}

TEST_CASE_METHOD(CollectionTest, "Delete All Collections in Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* colA = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(colA);
    
    CBLCollection* colB = CBLDatabase_CreateCollection(db, "colB"_sl, "scopeA"_sl, &error);
    REQUIRE(colB);
    
    CBLScope* scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    CBLScope_Retain(scope);
    
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
    
    // Check that the scope doesn't exist:
    CHECK(!CBLDatabase_Scope(db, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    scopeNames = CBLDatabase_ScopeNames(db, &error);
    CHECK(Array(scopeNames).toJSONString() == R"(["_default"])");
    FLMutableArray_Release(scopeNames);
    
    // Use the retained scope object:
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    CHECK(!CBLScope_Collection(scope, "colA"_sl, &error));
    CHECK(error.code == 0);
    CHECK(!CBLScope_Collection(scope, "colB"_sl, &error));
    CHECK(error.code == 0);
    colNames = CBLScope_CollectionNames(scope, &error);
    CHECK(Array(colNames).toJSONString() == R"([])");
    FLMutableArray_Release(colNames);
    CBLScope_Release(scope);
}

TEST_CASE_METHOD(CollectionTest, "Valid Collection and Scope Names", "[Collection]") {
    vector<string> names {
        "a", "B", "0", "-",
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_%"
    };

    for (auto name : names) {
        CBLError error = {};
        CBLCollection* col = CBLDatabase_CreateCollection(db, slice(name), slice(name), &error);
        REQUIRE(col);
        col = CBLDatabase_Collection(db, slice(name), slice(name), &error);
        CHECK(col == col);
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

TEST_CASE_METHOD(CollectionTest, "Collection Name Case Sensitive", "[.CBL-3195]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "COL1"_sl, "scopeA"_sl, &error);
    REQUIRE(col1a);
    
    CBLCollection* col1b = CBLDatabase_CreateCollection(db, "col1"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    
    CHECK(col1a != col1b);
    
    FLMutableArray colNames = CBLDatabase_CollectionNames(db, "scopeA"_sl, &error);
    CHECK(Array(colNames).toJSONString() == R"(["_default",COL1","col1"])");
    FLMutableArray_Release(colNames);
}

TEST_CASE_METHOD(CollectionTest, "Scope Name Case Sensitive", "[.CBL-3195]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "col1"_sl, "SCOPEA"_sl, &error);
    REQUIRE(col1a);
    
    CBLCollection* col1b = CBLDatabase_CreateCollection(db, "col1"_sl, "scopea"_sl, &error);
    REQUIRE(col1b);
    
    CHECK(col1a != col1b);
    
    FLMutableArray scopeNames = CBLDatabase_ScopeNames(db, &error);
    CHECK(Array(scopeNames).toJSONString() == R"(["_default",SCOPEA","scopea"])");
    FLMutableArray_Release(scopeNames);
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
    
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Delete then Get Collection from Different DB Instances", "[.CBL-3196]") {
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
    CHECK(CBLCollection_Count(col1a) == 0);
    CHECK(CBLCollection_Count(col1b) == 0);
    CHECK(!CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    CHECK(!CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Delete and Recreate then Get Collection from Different DB Instances", "[.CBL-3142][.CBL-3196]") {
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
    
    // Recreate:
    CBLCollection* col1c = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1c);
    CHECK(col1c != col1a);
    
    CHECK(CBLCollection_Count(col1a) == 0);
    CHECK(CBLCollection_Count(col1b) == 0);
    
    CBLCollection* col1d = CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1d);
    CHECK(col1d != col1b);
    
    CBLDatabase_Release(db2);
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection then Use Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    CBLCollection_Retain(col);
    
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    
    testInvalidCollection(col);
    
    CBLCollection_Release(col);
}

TEST_CASE_METHOD(CollectionTest, "Close Database then Use Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    CBLCollection_Retain(col);
    
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
    
    CBLCollection_Retain(col);
    
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
    
    CBLCollection_Retain(col);
    
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
    CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    
    REQUIRE(CBLDatabase_Close(db, &error));
    
    testInvalidDatabase(db);
    
    CBLDatabase_Release(db);
    db = nullptr;
}

TEST_CASE_METHOD(CollectionTest, "Delete Collection and Close Database then Use Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    
    CBLCollection_Retain(col);
    
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    
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
    
    CBLScope* scope = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    CBLScope_Retain(scope);
    
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == "scopeA"_sl);
    
    // Delete the collection in the scope:
    REQUIRE(CBLDatabase_DeleteCollection(db, "colA"_sl, "scopeA"_sl, &error));
    
    // Check that the scope doesn't exist:
    CHECK(!CBLDatabase_Scope(db, "scopeA"_sl, &error));
    CHECK(error.code == 0);
    
    // Close database:
    REQUIRE(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    db = nullptr;
    
    // Try to use the retained scope
    testInvalidScope(scope);
    
    CBLScope_Release(scope);
}
