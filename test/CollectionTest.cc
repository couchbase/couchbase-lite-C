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

static int defaultListenerCalls = 0;
static int fooListenerCalls = 0;
static int barListenerCalls = 0;
static int notificationsReadyCalls = 0;

static void defaultListener(void *context, const CBLCollectionChange* change) {
    ++defaultListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->defaultCollection == change->collection);
    CHECK(change -> numDocs == 1);
    CHECK(slice(change->docIDs[0]) == "foo"_sl);
}

static void defaultListener2(void *context, const CBLCollectionChange* change) {
    ++defaultListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->defaultCollection == change->collection);
    CHECK(change -> numDocs == 2);
    CHECK(slice(change->docIDs[0]) == "foo"_sl);
    CHECK(slice(change->docIDs[1]) == "bar"_sl);
}

static void fooListener(void *context, const CBLDocumentChange *change) {
    ++fooListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->defaultCollection == change->collection);
    CHECK(slice(change->docID) == "foo"_sl);
}

static void barListener(void *context, const CBLDocumentChange *change) {
    ++barListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->defaultCollection == change->collection);
    CHECK(slice(change->docID) == "bar"_sl);
}

static void notificationsReady(void *context, CBLDatabase* db) {
    ++notificationsReadyCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
}

class CollectionTest : public CBLTest {
    
public:
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
        
        CBLScope* scope = CBLCollection_Scope(col);
        CHECK(scope);
        CBLScope_Release(scope);
        
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
        CHECK(!CBLDatabase_DefaultScope(database, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_DefaultCollection(database, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_ScopeNames(database, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_CollectionNames(database, "_default"_sl,  &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_Collection(database, "_default"_sl, "_default"_sl, &error));
        CheckNotOpenError(error);
        
        error = {};
        CHECK(!CBLDatabase_Scope(database, "_default"_sl, &error));
        CheckNotOpenError(error);
    }
};

#define NOT_DELETE_DEFAULT_COLLECTION

TEST_CASE_METHOD(CollectionTest, "Default Collection", "[Collection]") {
    CHECK(CBLCollection_Name(defaultCollection) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(defaultCollection) == 0);
}

TEST_CASE_METHOD(CollectionTest, "Default Collection Exists By Default", "[Collection]") {
    CBLError error = {};
    REQUIRE(defaultCollection);
    CHECK(CBLCollection_Name(defaultCollection) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(defaultCollection) == 0);
    
    CBLScope* scope = CBLCollection_Scope(defaultCollection);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLScope_Release(scope);
    
    CBLCollection* col = CBLDatabase_Collection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == kCBLDefaultCollectionName);
    CHECK(CBLCollection_Count(col) == 0);
    
    scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLScope_Release(scope);
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

#ifndef NOT_DELETE_DEFAULT_COLLECTION

TEST_CASE_METHOD(CollectionTest, "Delete Default Collection", "[Collection]") {
    CBLError error = {};
    REQUIRE(defaultCollection);
    
    // Add some docs:
    createNumberedDocsWithPrefix(col, 100, "doc");
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
    REQUIRE(defaultCollection);
    
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

#else 

TEST_CASE_METHOD(CollectionTest, "Default Collection Cannot Be Deleted", "[Collection]") {
    ExpectingExceptions ex;
    CBLError error = {};
    REQUIRE(defaultCollection);
    
    // Try delete the default collection - should return false:
    REQUIRE(!CBLDatabase_DeleteCollection(db, kCBLDefaultCollectionName, kCBLDefaultScopeName, &error));
    CHECK((error.domain == kCBLDomain && error.code == kCBLErrorInvalidParameter ));
}

#endif
TEST_CASE_METHOD(CollectionTest, "Create And Get Collection In Default Scope", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col);
    CHECK(CBLCollection_Name(col) == "colA"_sl);
    
    CBLScope* scope = CBLCollection_Scope(col);
    REQUIRE(scope);
    CHECK(CBLScope_Name(scope) == kCBLDefaultScopeName);
    CBLScope_Release(scope);
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
    CBLScope_Release(scope);
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
    CBLScope_Release(scope);
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
    
    CBLCollection_Release(col1);
    CBLCollection_Release(col2);
}

TEST_CASE_METHOD(CollectionTest, "Get Non Existing Collection", "[Collection]") {
    CBLError error = {};
    CBLCollection* col = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    CHECK(col == nullptr);
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
    createNumberedDocsWithPrefix(col, 100, "doc");
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
    CHECK(CBLCollection_Name(colA2) == "colA"_sl);
    
    CBLCollection* colB2 = CBLScope_Collection(scope, "colB"_sl, &error);
    CHECK(CBLCollection_Name(colB2) == "colB"_sl);
    
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

TEST_CASE_METHOD(CollectionTest, "Collection Full Name", "[Collection]") {
    CBLError error = {};
    
    // 3.1 TestGetFullNameFromDefaultCollection
    CBLCollection* col1 = CBLDatabase_DefaultCollection(db, &error);
    REQUIRE(col1);
    CHECK(CBLCollection_FullName(col1) == "_default._default"_sl);
    CBLCollection_Release(col1);
    
    // 3.2 TestGetFullNameFromNewCollectionInDefaultScope
    CBLCollection* col2 = CBLDatabase_CreateCollection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col2);
    CHECK(CBLCollection_FullName(col2) == "_default.colA"_sl);
    CBLCollection_Release(col2);
    
    // 3.3 TestGetFullNameFromNewCollectionInCustomScope
    CBLCollection* col3 = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col3);
    CHECK(CBLCollection_FullName(col3) == "scopeA.colA"_sl);
    CBLCollection_Release(col3);
    
    // 3.4 TestGetFullNameFromExistingCollectionInDefaultScope
    CBLCollection* col4 = CBLDatabase_Collection(db, "colA"_sl, kCBLDefaultScopeName, &error);
    REQUIRE(col4);
    CHECK(CBLCollection_FullName(col4) == "_default.colA"_sl);
    CBLCollection_Release(col4);
    
    // 3.5 TestGetFullNameFromExistingCollectionInCustomScope
    CBLCollection* col5 = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col5);
    CHECK(CBLCollection_FullName(col5) == "scopeA.colA"_sl);
    CBLCollection_Release(col5);
}

TEST_CASE_METHOD(CollectionTest, "Collection Database", "[Collection]") {
    CBLError error = {};
    
    // 3.1 TestGetDatabaseFromNewCollection
    CBLCollection* col1 = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1);
    CHECK(CBLCollection_Database(col1) == db);
    CBLCollection_Release(col1);
    
    // 3.2 TestGetDatabaseFromExistingCollection
    CBLCollection* col2 = CBLDatabase_Collection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col2);
    CHECK(CBLCollection_Database(col2) == db);
    CBLCollection_Release(col2);
}

TEST_CASE_METHOD(CollectionTest, "Scope Database", "[Collection]") {
    CBLError error = {};
    
    // 3.3 TestGetDatabaseFromNewCollection
    CBLCollection* col1 = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1);
    CBLScope* scope1 = CBLCollection_Scope(col1);
    CHECK(CBLScope_Database(scope1) == db);
    CHECK(CBLScope_Database(scope1) == CBLCollection_Database(col1));
    CBLScope_Release(scope1);
    CBLCollection_Release(col1);
    
    // 3.4 TestGetDatabaseFromScopeObtainedFromDatabase
    CBLScope* scope2 = CBLDatabase_Scope(db, "scopeA"_sl, &error);
    CHECK(CBLScope_Database(scope2) == db);
    CBLScope_Release(scope2);
}

TEST_CASE_METHOD(CollectionTest, "Create then Get Collection using Different DB Instances", "[Collection]") {
    CBLError error = {};
    CBLCollection* col1a = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1a);
    createNumberedDocsWithPrefix(col1a, 10, "doc");
    CHECK(CBLCollection_Count(col1a) == 10);
    
    // Using another instance to get the collection:
    CBLDatabase* db2 = openDB();
    CBLCollection* col1b = CBLDatabase_Collection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    CHECK(col1a != col1b);
    CHECK(CBLCollection_Count(col1b) == 10);
    
    // Create another 10 docs in col1b:
    createNumberedDocsWithPrefix(col1b, 10, "doc", 100);
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
    createNumberedDocsWithPrefix(col1a, 10, "doc");
    CHECK(CBLCollection_Count(col1a) == 10);
    
    // Using another instance to create the collection again:
    CBLDatabase* db2 = openDB();
    CBLCollection* col1b = CBLDatabase_CreateCollection(db2, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col1b);
    CHECK(col1a != col1b);
    CHECK(CBLCollection_Count(col1b) == 10);
    
    // Create another 10 docs in col1b:
    createNumberedDocsWithPrefix(col1b, 10, "doc", 100);
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
    createNumberedDocsWithPrefix(col1a, 10, "doc");
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
    createNumberedDocsWithPrefix(col1a, 10, "doc");
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
    
    CBLScope_Release(scope);
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
    
    CBLScope_Release(scope);
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
    CBLCollection* col = CBLDatabase_CreateCollection(db, "colA"_sl, "scopeA"_sl, &error);
    REQUIRE(col);
    CBLCollection_Release(col);
    
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

#pragma mark - LISTENERS:

TEST_CASE_METHOD(CollectionTest, "Collection notifications") {
    // Add a listener:
    defaultListenerCalls = fooListenerCalls = 0;
    auto token = CBLCollection_AddChangeListener(defaultCollection, defaultListener, this);
    auto docToken = CBLCollection_AddDocumentChangeListener(defaultCollection, "foo"_sl, fooListener, this);

    // Create a doc, check that the listener was called:
    createDocWithPair(defaultCollection, "foo", "greeting", "Howdy!");
    CHECK(defaultListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    CBLListener_Remove(token);
    CBLListener_Remove(docToken);

    // After being removed, the listener should not be called:
    defaultListenerCalls = fooListenerCalls = 0;
    createDocWithPair(defaultCollection, "bar", "greeting", "yo.");
    CHECK(defaultListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
}

TEST_CASE_METHOD(CollectionTest, "Remove Collection Listener after releasing collection") {
    // Add a listener:
    defaultListenerCalls = fooListenerCalls = 0;
    auto token = CBLCollection_AddChangeListener(defaultCollection, defaultListener, this);
    auto docToken = CBLCollection_AddDocumentChangeListener(defaultCollection, "foo"_sl, fooListener, this);

    // Create a doc, check that the listener was called:
    createDocWithPair(defaultCollection, "foo", "greeting", "Howdy!");
    CHECK(defaultListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    CBLCollection_Release(defaultCollection);
    defaultCollection = nullptr;

    CBLListener_Remove(token);
    CBLListener_Remove(docToken);
}

TEST_CASE_METHOD(CollectionTest, "Remove Listeners After Closing Database", "[Document]") {
    // Add a listener:
    defaultListenerCalls  = fooListenerCalls = 0;
    auto token = CBLCollection_AddChangeListener(defaultCollection, defaultListener, this);
    auto docToken = CBLCollection_AddDocumentChangeListener(defaultCollection, "foo"_sl, fooListener, this);
    
    // Create a doc, check that the listener was called:
    createDocWithPair(defaultCollection, "foo", "greeting", "Howdy!");
    CHECK(defaultListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    // Close and release the database:
    CBLError error;
    if (!CBLDatabase_Close(db, &error))
        WARN("Failed to close database: " << error.domain << "/" << error.code);
    CBLDatabase_Release(db);
    db = nullptr;

    // Remove and release the token:
    ExpectingExceptions x;
    CBLListener_Remove(token);
    CBLListener_Remove(docToken);
}


TEST_CASE_METHOD(CollectionTest, "Scheduled collection notifications at database level") {
    // Add a listener:
    defaultListenerCalls = fooListenerCalls = barListenerCalls = 0;

    auto token = CBLCollection_AddChangeListener(defaultCollection, defaultListener2, this);
    auto fooToken = CBLCollection_AddDocumentChangeListener(defaultCollection, "foo"_sl, fooListener, this);
    auto barToken = CBLCollection_AddDocumentChangeListener(defaultCollection, "bar"_sl, barListener, this);

    CBLDatabase_BufferNotifications(db, notificationsReady, this);

    // Create two docs; no listeners should be called yet:
    createDocWithPair(defaultCollection, "foo", "greeting", "Howdy!");
    CHECK(notificationsReadyCalls == 1);
    CHECK(defaultListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    createDocWithPair(defaultCollection, "bar", "greeting", "yo.");
    CHECK(notificationsReadyCalls == 1);
    CHECK(defaultListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    // Now the listeners will be called:
    CBLDatabase_SendNotifications(db);
    CHECK(defaultListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    // There should be no more notifications:
    CBLDatabase_SendNotifications(db);
    CHECK(defaultListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    CBLListener_Remove(token);
    CBLListener_Remove(fooToken);
    CBLListener_Remove(barToken);
}

