//
// DatabaseTest.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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
#include <string>
#include <thread>

using namespace std;
using namespace fleece;


static constexpr const slice kOtherDBName = "CBLTest_OtherDB";

class DatabaseTest : public CBLTest {
public:
    CBLDatabase* otherDB = nullptr;
    
    DatabaseTest() {
        CBLError error;
        if (!CBL_DeleteDatabase(kOtherDBName, kDatabaseConfiguration.directory, &error) && error.code != 0)
            FAIL("Can't delete otherDB database: " << error.domain << "/" << error.code);
    }

    ~DatabaseTest() {
        if (otherDB) {
            CBLError error;
            if (!CBLDatabase_Close(otherDB, &error))
                WARN("Failed to close other database: " << error.domain << "/" << error.code);
            CBLDatabase_Release(otherDB);
        }
    }
};


static void createDocument(CBLDatabase *db, slice docID, slice property, slice value) {
    CBLDocument* doc = CBLDocument_CreateWithID(docID);
    MutableDict props = CBLDocument_MutableProperties(doc);
    FLSlot_SetString(FLMutableDict_Set(props, property), value);
    CBLError error;
    bool saved = CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlFailOnConflict, &error);
    CBLDocument_Release(doc);
    REQUIRE(saved);
}


TEST_CASE_METHOD(DatabaseTest, "Database") {
    CHECK(CBLDatabase_Name(db) == kDatabaseName);
    CHECK(string(CBLDatabase_Path(db)) == string(kDatabaseDir) + kPathSeparator + string(kDatabaseName) + ".cblite2" + kPathSeparator);
    CHECK(CBL_DatabaseExists(kDatabaseName, kDatabaseDir));
    CHECK(CBLDatabase_Count(db) == 0);
    CHECK(CBLDatabase_LastSequence(db) == 0);       // not public API
}


TEST_CASE_METHOD(DatabaseTest, "Database w/o config") {
    CBLError error;
    CBLDatabase *defaultdb = CBLDatabase_Open("unconfig"_sl, nullptr, &error);
    REQUIRE(defaultdb);
    alloc_slice path = CBLDatabase_Path(defaultdb);
    cerr << "Default database is at " << path << "\n";
    CHECK(CBL_DatabaseExists("unconfig"_sl, nullslice));

    CBLDatabaseConfiguration config = CBLDatabase_Config(defaultdb);
    CHECK(config.directory != nullslice);     // exact value is platform-specific
#ifdef COUCHBASE_ENTERPRISE
    CHECK(config.encryptionKey.algorithm == kCBLEncryptionNone);
#endif
    CHECK(CBLDatabase_Delete(defaultdb, &error));
    CBLDatabase_Release(defaultdb);

    CHECK(!CBL_DatabaseExists("unconfig"_sl, nullslice));
}


#ifdef COUCHBASE_ENTERPRISE

TEST_CASE_METHOD(DatabaseTest, "Database Encryption") {
    // Ensure no database:
    CBL_DeleteDatabase("encdb"_sl, nullslice, nullptr);
    CHECK(!CBL_DatabaseExists("encdb"_sl, nullslice));
    
    // Correct key:
    CBLError error;
    CBLEncryptionKey key;
    CBLEncryptionKey_FromPassword(&key, "sekrit"_sl);
    CBLDatabaseConfiguration config = {nullslice, key};
    CBLDatabase *defaultdb = CBLDatabase_Open("encdb"_sl, &config, &error);
    REQUIRE(defaultdb);
    alloc_slice path = CBLDatabase_Path(defaultdb);
    cerr << "Default database is at " << path << "\n";
    CHECK(CBL_DatabaseExists("encdb"_sl, nullslice));

    CBLDatabaseConfiguration config1 = CBLDatabase_Config(defaultdb);
    REQUIRE(config1.encryptionKey.algorithm  == key.algorithm);
    REQUIRE(memcmp(config1.encryptionKey.bytes, key.bytes, 32) == 0);
    
    // Correct key from config:
    CBLDatabase *correctkeydb = CBLDatabase_Open("encdb"_sl, &config1, &error);
    REQUIRE(correctkeydb);
    CBLDatabase_Release(correctkeydb);
    
    // No key:
    {
        ExpectingExceptions x;
        CBLDatabase *nokeydb = CBLDatabase_Open("encdb"_sl, nullptr, &error);
        REQUIRE(nokeydb == nullptr);
        CHECK(error.domain == kCBLDomain);
        CHECK(error.code == kCBLErrorNotADatabaseFile);
    }
    
    // Wrong key:
    {
        ExpectingExceptions x;
        CBLEncryptionKey key2;
        CBLEncryptionKey_FromPassword(&key2, "wrongpassword"_sl);
        CBLDatabaseConfiguration config2 = {nullslice, key2};
        CBLDatabase *wrongkeydb = CBLDatabase_Open("encdb"_sl, &config2, &error);
        REQUIRE(wrongkeydb == nullptr);
        CHECK(error.domain == kCBLDomain);
        CHECK(error.code == kCBLErrorNotADatabaseFile);
    }

    CHECK(CBLDatabase_Delete(defaultdb, &error));
    CBLDatabase_Release(defaultdb);
    CHECK(!CBL_DatabaseExists("encdb"_sl, nullslice));
}

#endif


#pragma mark - Document:


TEST_CASE_METHOD(DatabaseTest, "Missing Document") {
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "foo"_sl, &error);
    CHECK(doc == nullptr);
    CHECK(error.code == 0);

    CBLDocument* mdoc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(mdoc == nullptr);
    CHECK(error.code == 0);

    CHECK(!CBLDatabase_PurgeDocumentByID(db, "foo"_sl, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
}


TEST_CASE_METHOD(DatabaseTest, "New Document") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    CHECK(doc != nullptr);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_RevisionID(doc) == nullslice);
    CHECK(CBLDocument_Sequence(doc) == 0);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{}"_sl);
    CHECK(CBLDocument_MutableProperties(doc) == CBLDocument_Properties(doc));
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "New Document With Auto ID") {
    CBLDocument* doc = CBLDocument_Create();
    CHECK(doc != nullptr);
    CHECK(CBLDocument_ID(doc) != nullslice);
    CHECK(CBLDocument_RevisionID(doc) == nullslice);
    CHECK(CBLDocument_Sequence(doc) == 0);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{}"_sl);
    CHECK(CBLDocument_MutableProperties(doc) == CBLDocument_Properties(doc));
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Mutable Copy New Document") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    CHECK(doc != nullptr);
    MutableDict props = CBLDocument_MutableProperties(doc);
    props["greeting"_sl] = "Howdy!"_sl;
    
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_RevisionID(doc) == nullslice);
    CHECK(CBLDocument_Sequence(doc) == 0);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    
    CBLDocument* mDoc = CBLDocument_MutableCopy(doc);
    CHECK(mDoc != doc);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(mDoc) == 0);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    
    CBLDocument_Release(doc);
    CBLDocument_Release(mDoc);
}


TEST_CASE_METHOD(DatabaseTest, "Mutable Copy Immutable Document") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    CHECK(doc != nullptr);
    MutableDict props = CBLDocument_MutableProperties(doc);
    props["greeting"_sl] = "Howdy!"_sl;
    
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    
    const CBLDocument* rDoc = CBLDatabase_GetDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(rDoc) == "foo"_sl);
    CHECK(CBLDocument_RevisionID(rDoc) != nullslice);
    CHECK(CBLDocument_Sequence(rDoc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(rDoc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(rDoc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");

    CBLDocument* mDoc = CBLDocument_MutableCopy(rDoc);
    CHECK(mDoc != rDoc);
    CHECK(CBLDocument_ID(mDoc) == "foo"_sl);
    CHECK(CBLDocument_RevisionID(mDoc) == CBLDocument_RevisionID(rDoc));
    CHECK(CBLDocument_Sequence(mDoc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    
    CBLDocument_Release(doc);
    CBLDocument_Release(rDoc);
    CBLDocument_Release(mDoc);
}


TEST_CASE_METHOD(DatabaseTest, "Access nested collections from mutable props") {
    CBLError error;
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    REQUIRE(doc);
    REQUIRE(CBLDocument_SetJSON(doc, "{\"name\":{\"first\": \"Jane\"}, \"phones\": [\"650-123-4567\"]}"_sl, &error));
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    
    CBLDocument* mDoc = nullptr;
    FLMutableDict mProps = nullptr;
    
    // Note: When a mutable properties of a document is created, the shallow copy from
    // the original properties will be made.
    
    SECTION("A mutable doc") {
        mDoc = doc;
        CBLDocument_Retain(doc);
    }
    
    SECTION("A mutable doc read from database") {
        mDoc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    }
    
    SECTION("Mutable copy from an immutable doc") {
        const CBLDocument* doc1 = CBLDatabase_GetDocument(db, "foo"_sl, &error);
        mDoc = CBLDocument_MutableCopy(doc1);
        CBLDocument_Release(doc1);
    }

    SECTION("Mutable copy from a mutable doc") {
        CBLDocument* mDoc1 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
        mDoc = CBLDocument_MutableCopy(mDoc1);
        CBLDocument_Release(mDoc1);
    }
    
    mProps = CBLDocument_MutableProperties(mDoc);
    
    // Dict:
    FLDict dict = FLValue_AsDict(FLDict_Get(mProps, "name"_sl));
    REQUIRE(dict);
    CHECK(FLDict_Count(dict) == 1);
    CHECK(FLValue_AsString(FLDict_Get(dict, "first"_sl)) == "Jane"_sl);
    FLMutableDict mDict = FLDict_AsMutable(dict); // Immutable
    CHECK(!mDict);
    
    mDict = FLMutableDict_GetMutableDict(mProps, "name"_sl);
    REQUIRE(mDict);
    CHECK(FLDict_Count(mDict) == 1);
    CHECK(FLValue_AsString(FLDict_Get(mDict, "first"_sl)) == "Jane"_sl);
    
    // Array:
    FLArray array = FLValue_AsArray(FLDict_Get(mProps, "phones"_sl));
    REQUIRE(array);
    REQUIRE(FLArray_Count(array) == 1);
    CHECK(FLValue_AsString(FLArray_Get(array, 0)) == "650-123-4567"_sl);
    FLMutableArray mArray = FLArray_AsMutable(array); // Immutable
    CHECK(!mArray);
    
    mArray = FLMutableDict_GetMutableArray(mProps, "phones"_sl);
    REQUIRE(mArray);
    REQUIRE(FLArray_Count(mArray) == 1);
    CHECK(FLValue_AsString(FLArray_Get(mArray, 0)) == "650-123-4567"_sl);
    
    CBLDocument_Release(mDoc);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Access nested collections from a copy of modified mutable doc") {
    CBLError error;
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    REQUIRE(doc);
    REQUIRE(CBLDocument_SetJSON(doc, "{\"name\":{\"first\": \"Jane\"}, \"phones\": [\"650-123-4567\"]}"_sl, &error));
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    
    FLMutableDict mProps = CBLDocument_MutableProperties(doc);
    
    // Modify Dict:
    FLMutableDict mDict = FLMutableDict_GetMutableDict(mProps, "name"_sl);
    REQUIRE(mDict);
    CHECK(FLDict_Count(mDict) == 1);
    CHECK(FLValue_AsString(FLDict_Get(mDict, "first"_sl)) == "Jane"_sl);
    FLMutableDict_SetString(mDict, "first"_sl, "Julie"_sl);
    CHECK(FLValue_AsString(FLDict_Get(mDict, "first"_sl)) == "Julie"_sl);
    
    // Modify Array:
    FLMutableArray mArray = FLMutableDict_GetMutableArray(mProps, "phones"_sl);
    REQUIRE(mArray);
    REQUIRE(FLArray_Count(mArray) == 1);
    CHECK(FLValue_AsString(FLArray_Get(mArray, 0)) == "650-123-4567"_sl);
    FLMutableArray_SetString(mArray, 0, "415-123-4567"_sl);
    CHECK(FLValue_AsString(FLArray_Get(mArray, 0)) == "415-123-4567"_sl);
    
    // Copy:
    CBLDocument* mDoc = CBLDocument_MutableCopy(doc);
    mProps = CBLDocument_MutableProperties(mDoc);
    
    // Check Dict:
    FLDict dict = FLValue_AsDict(FLDict_Get(mProps, "name"_sl));
    REQUIRE(dict);
    CHECK(FLDict_Count(dict) == 1);
    CHECK(FLValue_AsString(FLDict_Get(dict, "first"_sl)) == "Julie"_sl);
    FLMutableDict mDict2 = FLDict_AsMutable(dict); // Already mutable
    CHECK(mDict2);
    
    mDict2 = FLMutableDict_GetMutableDict(mProps, "name"_sl);
    REQUIRE(mDict2);
    CHECK(FLDict_Count(mDict2) == 1);
    CHECK(FLValue_AsString(FLDict_Get(mDict2, "first"_sl)) == "Julie"_sl);
    CHECK(mDict2 != mDict);
    
    // Check Array:
    FLArray array = FLValue_AsArray(FLDict_Get(mProps, "phones"_sl));
    REQUIRE(array);
    REQUIRE(FLArray_Count(array) == 1);
    CHECK(FLValue_AsString(FLArray_Get(array, 0)) == "415-123-4567"_sl);
    FLArray mArray2 = FLArray_AsMutable(array);
    CHECK(mArray2); // Already mutable
    
    mArray2 = FLMutableDict_GetMutableArray(mProps, "phones"_sl);
    REQUIRE(mArray2);
    REQUIRE(FLArray_Count(mArray2) == 1);
    CHECK(FLValue_AsString(FLArray_Get(mArray2, 0)) == "415-123-4567"_sl);
    CHECK(mArray2 != mArray);
    
    CBLDocument_Release(mDoc);
    CBLDocument_Release(doc);
}

TEST_CASE_METHOD(DatabaseTest, "Set MutableProperties") {
    CBLDocument* doc1 = CBLDocument_Create();
    FLMutableDict prop1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(prop1, "greeting"_sl, "hello"_sl);
    
    CBLDocument* doc2 = CBLDocument_Create();
    CBLDocument_SetProperties(doc2, prop1);
    FLMutableDict prop2 = CBLDocument_MutableProperties(doc2);
    CHECK(FLValue_AsString(FLDict_Get(prop2, "greeting"_sl)) == "hello"_sl );
    CHECK(prop1 == prop2);
    
    CBLDocument_Release(doc1);
    CHECK(FLValue_AsString(FLDict_Get(prop2, "greeting"_sl)) == "hello"_sl );
    CBLDocument_Release(doc2);
}

TEST_CASE_METHOD(DatabaseTest, "Get Non Existing Document") {
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "foo"_sl, &error);
    REQUIRE(doc == nullptr);
    CHECK(error.code == 0);
}

TEST_CASE_METHOD(DatabaseTest, "Get Document with Empty ID") {
    CBLError error;
    ExpectingExceptions x;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, ""_sl, &error);
    REQUIRE(doc == nullptr);
    CHECK(error.code == 0);
}


#pragma mark - Save Document:


TEST_CASE_METHOD(DatabaseTest, "Save Empty Document") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{}"_sl);
    CBLDocument_Release(doc);

    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_RevisionID(doc) == "1-581ad726ee407c8376fc94aad966051d013893c4"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{}"_sl);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document With Property") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    MutableDict props = CBLDocument_MutableProperties(doc);
    props["greeting"_sl] = "Howdy!"_sl;
    // or alternatively:  FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");

    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);

    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document Twice") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    MutableDict props = CBLDocument_MutableProperties(doc);
    props["greeting"_sl] = "Howdy!"_sl;
    
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");

    CBLDocument* savedDoc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(savedDoc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(savedDoc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(savedDoc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(savedDoc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(savedDoc);
    
    // Modify Again:
    props = CBLDocument_MutableProperties(doc);
    props["greeting"_sl] = "Hello!"_sl;
    
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 2);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Hello!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Hello!\"}");
    CBLDocument_Release(doc);

    savedDoc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(savedDoc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(savedDoc) == 2);
    CHECK(alloc_slice(CBLDocument_CreateJSON(savedDoc)) == "{\"greeting\":\"Hello!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(savedDoc)).toJSONString() == "{\"greeting\":\"Hello!\"}");
    CBLDocument_Release(savedDoc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document with LastWriteWin") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);

    CBLError error;
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);

    CBLDocument* doc1 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc1) == 1);
    
    CBLDocument* doc2 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc2) == 1);
    
    FLMutableDict props1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(props1, "name"_sl, "bob"_sl);
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc1, kCBLConcurrencyControlLastWriteWins, &error));
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CBLDocument_Release(doc1);
    
    FLMutableDict props2 = CBLDocument_MutableProperties(doc2);
    FLMutableDict_SetString(props2, "name"_sl, "sally"_sl);
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc2, kCBLConcurrencyControlLastWriteWins, &error));
    CHECK(CBLDocument_Sequence(doc2) == 3);
    CBLDocument_Release(doc2);
    
    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 3);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\",\"name\":\"sally\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"sally\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document with FailOnConflict") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);

    CBLError error;
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlFailOnConflict, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);

    CBLDocument* doc1 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc1) == 1);
    
    CBLDocument* doc2 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc2) == 1);
    
    FLMutableDict props1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(props1, "name"_sl, "bob"_sl);
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc1, kCBLConcurrencyControlFailOnConflict, &error));
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CBLDocument_Release(doc1);
    
    FLMutableDict props2 = CBLDocument_MutableProperties(doc2);
    FLMutableDict_SetString(props2, "name"_sl, "sally"_sl);
    CHECK(!CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc2, kCBLConcurrencyControlFailOnConflict, &error));
    CBLDocument_Release(doc2);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorConflict);
    
    error = {};
    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 2);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\",\"name\":\"bob\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"bob\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document with Conflict Handler") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    
    CBLConflictHandler failConflict = [](void *c, CBLDocument *mine, const CBLDocument *existing) -> bool {
        CHECK(!c);
        return false;
    };
    
    CBLConflictHandler mergeConflict = [](void *c, CBLDocument *mine, const CBLDocument *existing) -> bool {
        FLMutableDict mergedProps = CBLDocument_MutableProperties(mine);
        FLValue anotherName = FLDict_Get(CBLDocument_Properties(existing),"name"_sl);
        FLMutableDict_SetValue(mergedProps, "anotherName"_sl, anotherName);
        return true;
    };
    
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocumentWithConflictHandler(db, doc, failConflict, nullptr, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);

    CBLDocument* doc1 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc1) == 1);
    
    CBLDocument* doc2 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc2) == 1);
    
    FLMutableDict props1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(props1, "name"_sl, "bob"_sl);
    REQUIRE(CBLDatabase_SaveDocumentWithConflictHandler(db, doc1, failConflict, nullptr, &error));
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CBLDocument_Release(doc1);
    
    FLMutableDict props2 = CBLDocument_MutableProperties(doc2);
    FLMutableDict_SetString(props2, "name"_sl, "sally"_sl);
    CHECK(!CBLDatabase_SaveDocumentWithConflictHandler(db, doc2, failConflict, nullptr, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorConflict);
    
    error = {};
    CHECK(CBLDatabase_SaveDocumentWithConflictHandler(db, doc2, mergeConflict, nullptr, &error));
    CBLDocument_Release(doc2);
    
    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 3);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\",\"name\":\"sally\",\"anotherName\":\"bob\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"sally\",\"anotherName\":\"bob\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document with Conflict Handler : Called twice") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    
    CBLConflictHandler mergeConflict = [](void *c, CBLDocument *mine, const CBLDocument *existing) -> bool {
        CHECK(c != nullptr);
        CBLDatabase *theDB = (CBLDatabase*)c;
        Dict dict = (CBLDocument_Properties(existing));
        if (dict["name"].asString() == "bob"_sl) {
            // Update the doc to cause a new conflict after first merge; the handler will be
            // called again:
            CHECK(CBLDatabase_LastSequence(theDB) == 2);
            CBLError e;
            CBLDocument* doc3 = CBLDatabase_GetMutableDocument(theDB, "foo"_sl, &e);
            FLMutableDict props3 = CBLDocument_MutableProperties(doc3);
            FLMutableDict_SetString(props3, "name"_sl, "max"_sl);
            REQUIRE(CBLDatabase_SaveDocument(theDB, doc3, &e));
            CBLDocument_Release(doc3);
            CHECK(CBLDatabase_LastSequence(theDB) == 3);
        } else {
            CHECK(CBLDatabase_LastSequence(theDB) == 3);
            CHECK(dict["name"].asString() == "max"_sl);
        }
        
        FLMutableDict mergedProps = CBLDocument_MutableProperties(mine);
        FLMutableDict_SetValue(mergedProps, "anotherName"_sl, dict["name"]);
        return true;
    };
    
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);

    CBLDocument* doc1 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc1) == 1);
    
    CBLDocument* doc2 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc2) == 1);
    
    FLMutableDict props1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(props1, "name"_sl, "bob"_sl);
    REQUIRE(CBLDatabase_SaveDocument(db, doc1, &error));
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CBLDocument_Release(doc1);
    
    FLMutableDict props2 = CBLDocument_MutableProperties(doc2);
    FLMutableDict_SetString(props2, "name"_sl, "sally"_sl);
    CHECK(CBLDatabase_SaveDocumentWithConflictHandler(db, doc2, mergeConflict, db, &error));
    CBLDocument_Release(doc2);
    
    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 4);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\",\"name\":\"sally\",\"anotherName\":\"max\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"sally\",\"anotherName\":\"max\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document with Conflict Handler : On another thread") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    
    CBLConflictHandler mergeConflict = [](void *c, CBLDocument *myDoc, const CBLDocument *existingDoc) -> bool {
        // Shouldn't have deadlock when accessing document or database properties
        CHECK(c != nullptr);
        CHECK(CBLDocument_Sequence(myDoc) > 0);
        CHECK(CBLDatabase_LastSequence((CBLDatabase*)c) > 0);
        
        // Resolve in a different thread
        thread t([](CBLDatabase* db, CBLDocument *mine, const CBLDocument *existing) {
            // Shouldn't have deadlock when accessing document or database properties
            CHECK(CBLDocument_Sequence(mine) > 0);
            CHECK(CBLDatabase_LastSequence(db) > 0);
            FLMutableDict mergedProps = CBLDocument_MutableProperties(mine);
            FLMutableDict_SetString(mergedProps, "name"_sl, "max"_sl);
        }, (CBLDatabase*)c, myDoc, existingDoc);
        t.join();
        
        return true;
    };
    
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 1);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\"}");
    CBLDocument_Release(doc);

    CBLDocument* doc1 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc1) == 1);
    
    CBLDocument* doc2 = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc2) == 1);
    
    FLMutableDict props1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(props1, "name"_sl, "bob"_sl);
    REQUIRE(CBLDatabase_SaveDocument(db, doc1, &error));
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CBLDocument_Release(doc1);
    
    FLMutableDict props2 = CBLDocument_MutableProperties(doc2);
    FLMutableDict_SetString(props2, "name"_sl, "sally"_sl);
    CHECK(CBLDatabase_SaveDocumentWithConflictHandler(db, doc2, mergeConflict, db, &error));
    CBLDocument_Release(doc2);
    
    doc = CBLDatabase_GetMutableDocument(db, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(CBLDocument_Sequence(doc) == 3);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\",\"name\":\"max\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"greeting\":\"Howdy!\",\"name\":\"max\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document into Different DB") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    
    CBLError error;
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    
    otherDB = CBLDatabase_Open(kOtherDBName, &kDatabaseConfiguration, &error);
    REQUIRE(otherDB);
    
    ExpectingExceptions x;
    CHECK(!CBLDatabase_SaveDocument(otherDB, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save Document into Different DB Instance") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    
    CBLError error;
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    
    CBLDatabase* db2 = CBLDatabase_Open(kDatabaseName, &kDatabaseConfiguration, &error);
    REQUIRE(db2);
    
    ExpectingExceptions x;
    CHECK(!CBLDatabase_SaveDocument(db2, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
    
    error = {};
    REQUIRE(CBLDatabase_Close(db2, &error));
    CBLDatabase_Release(db2);
}


#pragma mark - Delete Document:


TEST_CASE_METHOD(DatabaseTest, "Delete Non Existing Document") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    
    ExpectingExceptions x;
    CBLError error;
    CHECK(!CBLDatabase_DeleteDocument(db, doc, &error));
    CBLDocument_Release(doc);
    
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
    
    error = {};
    CHECK(!CBLDatabase_DeleteDocumentByID(db, "foo"_sl, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
}


TEST_CASE_METHOD(DatabaseTest, "Delete Document") {
    createDocument(db, "doc1", "foo", "bar");
    createDocument(db, "doc2", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    CHECK(CBLDocument_Sequence(doc) == 1);
    
    CHECK(CBLDatabase_DeleteDocument(db, doc, &error));
    CHECK(CBLDocument_Sequence(doc) == 3);
    CBLDocument_Release(doc);
    CHECK(!CBLDatabase_GetDocument(db, "doc1"_sl, &error));
    
    CHECK(CBLDatabase_DeleteDocumentByID(db, "doc2"_sl, &error));
    CHECK(!CBLDatabase_GetDocument(db, "doc2"_sl, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Delete Already Deleted Document") {
    createDocument(db, "doc1", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    
    CHECK(CBLDatabase_DeleteDocument(db, doc, &error));
    CHECK(CBLDocument_Sequence(doc) == 2);
    CHECK(!CBLDatabase_GetDocument(db, "doc1"_sl, &error));
    
    CHECK(CBLDatabase_DeleteDocument(db, doc, &error));
    CHECK(CBLDocument_Sequence(doc) == 3);
    CBLDocument_Release(doc);
    
    CHECK(CBLDatabase_DeleteDocumentByID(db, "doc1"_sl, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Delete Then Update Document") {
    createDocument(db, "doc1", "foo", "bar");
    
    CBLError error;
    CBLDocument* doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    
    CHECK(CBLDatabase_DeleteDocument(db, doc, &error));
    CHECK(CBLDocument_Sequence(doc) == 2);
    CHECK(!CBLDatabase_GetDocument(db, "doc1"_sl, &error));
    
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "foo"_sl, "bar1"_sl);
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    
    CHECK(CBLDocument_ID(doc) == "doc1"_sl);
    CHECK(CBLDocument_Sequence(doc) == 3);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"foo\":\"bar1\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc)).toJSONString() == "{\"foo\":\"bar1\"}");
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Delete Document with LastWriteWin") {
    createDocument(db, "doc1", "foo", "bar");

    CBLError error;
    CBLDocument* doc1 = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "doc1"_sl);
    CHECK(CBLDocument_Sequence(doc1) == 1);
    
    CBLDocument* doc2 = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "doc1"_sl);
    CHECK(CBLDocument_Sequence(doc2) == 1);
    
    FLMutableDict props1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(props1, "foo"_sl, "bar1"_sl);
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc1, kCBLConcurrencyControlLastWriteWins, &error));
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CBLDocument_Release(doc1);
    
    REQUIRE(CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc2, kCBLConcurrencyControlLastWriteWins, &error));
    CHECK(CBLDocument_Sequence(doc2) == 3);
    CBLDocument_Release(doc2);
    
    REQUIRE(!CBLDatabase_GetDocument(db, "doc1"_sl, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Delete Document with FailOnConflict") {
    createDocument(db, "doc1", "foo", "bar");

    CBLError error;
    CBLDocument* doc1 = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "doc1"_sl);
    CHECK(CBLDocument_Sequence(doc1) == 1);
    
    CBLDocument* doc2 = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "doc1"_sl);
    CHECK(CBLDocument_Sequence(doc2) == 1);
    
    FLMutableDict props1 = CBLDocument_MutableProperties(doc1);
    FLMutableDict_SetString(props1, "foo"_sl, "bar1"_sl);
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc1, kCBLConcurrencyControlFailOnConflict, &error));
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CBLDocument_Release(doc1);
    
    REQUIRE(!CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc2, kCBLConcurrencyControlFailOnConflict, &error));
    CBLDocument_Release(doc2);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorConflict);
    
    error = {};
    doc1 = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    REQUIRE(doc1);
    CHECK(CBLDocument_Sequence(doc1) == 2);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc1)) == "{\"foo\":\"bar1\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc1)).toJSONString() == "{\"foo\":\"bar1\"}");
    CBLDocument_Release(doc1);
}


TEST_CASE_METHOD(DatabaseTest, "Delete Document from Different DB") {
    createDocument(db, "doc1", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    
    otherDB = CBLDatabase_Open(kOtherDBName, &kDatabaseConfiguration, &error);
    REQUIRE(otherDB);
    
    ExpectingExceptions x;
    CHECK(!CBLDatabase_DeleteDocument(otherDB, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Delete Document from Different DB Instance") {
    createDocument(db, "doc1", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    
    CBLDatabase* db2 = CBLDatabase_Open(kDatabaseName, &kDatabaseConfiguration, &error);
    REQUIRE(db2);
    
    ExpectingExceptions x;
    CHECK(!CBLDatabase_DeleteDocument(db2, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
    
    error = {};
    REQUIRE(CBLDatabase_Close(db2, &error));
    CBLDatabase_Release(db2);
}


#pragma mark - Purge Document:


TEST_CASE_METHOD(DatabaseTest, "Purge Non Existing Document") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    
    ExpectingExceptions x;
    CBLError error;
    CHECK(!CBLDatabase_PurgeDocument(db, doc, &error));
    CBLDocument_Release(doc);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
    
    error = {};
    CHECK(!CBLDatabase_PurgeDocumentByID(db, "foo"_sl, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
}


TEST_CASE_METHOD(DatabaseTest, "Purge Document") {
    createDocument(db, "doc1", "foo", "bar");
    createDocument(db, "doc2", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    CHECK(CBLDocument_Sequence(doc) == 1);
    
    CHECK(CBLDatabase_PurgeDocument(db, doc, &error));
    CBLDocument_Release(doc);
    CHECK(!CBLDatabase_GetDocument(db, "doc1"_sl, &error));
    
    CHECK(CBLDatabase_PurgeDocumentByID(db, "doc2"_sl, &error));
    CHECK(!CBLDatabase_GetDocument(db, "doc2"_sl, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Purge Already Purged Document") {
    createDocument(db, "doc1", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    
    CHECK(CBLDatabase_PurgeDocument(db, doc, &error));
    CHECK(!CBLDatabase_GetDocument(db, "doc1"_sl, &error));
    
    CHECK(!CBLDatabase_PurgeDocument(db, doc, &error));
    CBLDocument_Release(doc);
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
    
    error = {};
    CHECK(!CBLDatabase_PurgeDocumentByID(db, "doc1"_sl, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorNotFound);
}


TEST_CASE_METHOD(DatabaseTest, "Purge Document from Different DB") {
    createDocument(db, "doc1", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    
    otherDB = CBLDatabase_Open(kOtherDBName, &kDatabaseConfiguration, &error);
    REQUIRE(otherDB);
    
    ExpectingExceptions x;
    CHECK(!CBLDatabase_PurgeDocument(otherDB, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Purge Document from Different DB Instance") {
    createDocument(db, "doc1", "foo", "bar");
    
    CBLError error;
    const CBLDocument* doc = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    
    CBLDatabase* db2 = CBLDatabase_Open(kDatabaseName, &kDatabaseConfiguration, &error);
    REQUIRE(db2);
    
    ExpectingExceptions x;
    CHECK(!CBLDatabase_PurgeDocument(db2, doc, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    CBLDocument_Release(doc);
    
    error = {};
    REQUIRE(CBLDatabase_Close(db2, &error));
    CBLDatabase_Release(db2);
}


#pragma mark - File Operations:


TEST_CASE_METHOD(DatabaseTest, "Copy Database") {
    CBLDocument* doc = CBLDocument_CreateWithID("foo"_sl);
    FLMutableDict props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "greeting"_sl, "Howdy!"_sl);
    
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CBLDocument_Release(doc);
    
    REQUIRE(!CBL_DatabaseExists(kOtherDBName, kDatabaseConfiguration.directory));
    
    // Copy:
    alloc_slice path = CBLDatabase_Path(db);
    REQUIRE(CBL_CopyDatabase(path, kOtherDBName, &kDatabaseConfiguration, &error));
    
    // Check:
    CHECK(CBL_DatabaseExists(kOtherDBName, kDatabaseConfiguration.directory));
    otherDB = CBLDatabase_Open(kOtherDBName, &kDatabaseConfiguration, &error);
    CHECK(otherDB != nullptr);
    CHECK(CBLDatabase_Count(otherDB) == 1);
    
    doc = CBLDatabase_GetMutableDocument(otherDB, "foo"_sl, &error);
    CHECK(CBLDocument_ID(doc) == "foo"_sl);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc)) == "{\"greeting\":\"Howdy!\"}"_sl);
    CBLDocument_Release(doc);
}


#pragma mark - Document Expiry:


TEST_CASE_METHOD(DatabaseTest, "Expiration") {
    createDocument(db, "doc1", "foo", "bar");
    createDocument(db, "doc2", "foo", "bar");
    createDocument(db, "doc3", "foo", "bar");

    CBLError error;
    CBLTimestamp future = CBL_Now() + 1000;
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc1"_sl, future, &error));
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc3"_sl, future, &error));
    CHECK(CBLDatabase_Count(db) == 3);

    CHECK(CBLDatabase_GetDocumentExpiration(db, "doc1"_sl, &error) == future);
    CHECK(CBLDatabase_GetDocumentExpiration(db, "doc2"_sl, &error) == 0);
    CHECK(CBLDatabase_GetDocumentExpiration(db, "docX"_sl, &error) == 0);

    this_thread::sleep_for(1700ms);
    CHECK(CBLDatabase_Count(db) == 1);
}


TEST_CASE_METHOD(DatabaseTest, "Expiration After Reopen") {
    createDocument(db, "doc1", "foo", "bar");
    createDocument(db, "doc2", "foo", "bar");
    createDocument(db, "doc3", "foo", "bar");

    CBLError error;
    CBLTimestamp future = CBL_Now() + 2000;
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc1"_sl, future, &error));
    CHECK(CBLDatabase_SetDocumentExpiration(db, "doc3"_sl, future, &error));
    CHECK(CBLDatabase_Count(db) == 3);

    // Close & reopen the database:
    REQUIRE(CBLDatabase_Close(db, &error));
    CBLDatabase_Release(db);
    db = CBLDatabase_Open(kDatabaseName, &kDatabaseConfiguration, &error);

    // Now wait for expiration:
    this_thread::sleep_for(3000ms);
    CHECK(CBLDatabase_Count(db) == 1);
}


#pragma mark - Maintenance:


TEST_CASE_METHOD(DatabaseTest, "Maintenance : Compact and Integrity Check") {
    // Create a doc with blob:
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict dict = CBLDocument_MutableProperties(doc);
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob *blob1 = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    FLMutableDict_SetBlob(dict, FLStr("blob"), blob1);
    
    // Save doc:
    CBLError error;
    REQUIRE(CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLBlob_Release(blob1);
    CBLDocument_Release(doc);
    
    // Compact:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    
    // Make sure the blob still exists after compact: (issue #73)
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(CBLDocument_Properties(doc), FLStr("blob")));
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    FLSliceResult_Release(content);
    
    // https://issues.couchbase.com/browse/CBL-1617
    // CBLBlob_Release(blob2);
    
    // Delete doc:
    CHECK(CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc, kCBLConcurrencyControlLastWriteWins, &error));
    CBLDocument_Release(doc);
    
    // Compact:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    
    // Integrity check:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeIntegrityCheck, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Maintenance : Reindex") {
    CBLError error;
    CBLValueIndexConfiguration config = {};
    config.expressionLanguage = kCBLJSONLanguage;
    config.expressions = R"(["foo"])"_sl;
    CHECK(CBLDatabase_CreateValueIndex(db, "foo"_sl, config, &error));
    
    createDocument(db, "doc1", "foo", "bar1");
    createDocument(db, "doc2", "foo", "bar2");
    createDocument(db, "doc3", "foo", "bar3");
    
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeReindex, &error));
    
    FLArray names = CBLDatabase_GetIndexNames(db);
    REQUIRE(names);
    CHECK(FLArray_Count(names) == 1);
    CHECK(FLValue_AsString(FLArray_Get(names, 0)) == "foo"_sl);
    FLArray_Release(names);
}


TEST_CASE_METHOD(DatabaseTest, "Maintenance : Optimize") {
    CBLValueIndexConfiguration index1 = {};
    index1.expressionLanguage = kCBLN1QLLanguage;
    index1.expressions = "name.first"_sl;
    CBLError error;
    CHECK(CBLDatabase_CreateValueIndex(db, "index1"_sl, index1, &error));
    
    ImportJSONLines(GetTestFilePath("names_100.json"), db);
    
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeOptimize, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Maintenance : FullOptimize") {
    CBLValueIndexConfiguration index1 = {};
    index1.expressionLanguage = kCBLN1QLLanguage;
    index1.expressions = "name.first"_sl;
    CBLError error;
    CHECK(CBLDatabase_CreateValueIndex(db, "index1"_sl, index1, &error));
    
    ImportJSONLines(GetTestFilePath("names_100.json"), db);
    
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeFullOptimize, &error));
}


#pragma mark - Transaction:


TEST_CASE_METHOD(DatabaseTest, "Transaction Commit") {
    createDocument(db, "doc1", "foo", "bar1");
    createDocument(db, "doc2", "foo", "bar2");

    CHECK(CBLDatabase_Count(db) == 2);

    // Begin Transaction:
    CBLError error;
    REQUIRE(CBLDatabase_BeginTransaction(db, &error));

    // Create:
    createDocument(db, "doc3", "foo", "bar3");

    // Delete:
    const CBLDocument* doc1 = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc1);
    REQUIRE(CBLDatabase_DeleteDocument(db, doc1, &error));
    CBLDocument_Release(doc1);

    // Purge:
    const CBLDocument* doc2 = CBLDatabase_GetDocument(db, "doc2"_sl, &error);
    REQUIRE(doc2);
    REQUIRE(CBLDatabase_PurgeDocument(db, doc2, &error));
    CBLDocument_Release(doc2);

    // Commit Transaction:
    REQUIRE(CBLDatabase_EndTransaction(db, true, &error));

    // Check:
    CHECK(CBLDatabase_Count(db) == 1);
    const CBLDocument* doc3 = CBLDatabase_GetDocument(db, "doc3"_sl, &error);
    CHECK(CBLDocument_ID(doc3) == "doc3"_sl);
    CHECK(CBLDocument_Sequence(doc3) == 3);
    CHECK(alloc_slice(CBLDocument_CreateJSON(doc3)) == "{\"foo\":\"bar3\"}"_sl);
    CHECK(Dict(CBLDocument_Properties(doc3)).toJSONString() == "{\"foo\":\"bar3\"}");
    CBLDocument_Release(doc3);

    CHECK(!CBLDatabase_GetDocument(db, "doc1"_sl, &error));
    CHECK(!CBLDatabase_GetDocument(db, "doc2"_sl, &error));
}


TEST_CASE_METHOD(DatabaseTest, "Transaction Abort") {
    createDocument(db, "doc1", "foo", "bar1");
    createDocument(db, "doc2", "foo", "bar2");
    
    // Begin Transaction:
    CBLError error;
    REQUIRE(CBLDatabase_BeginTransaction(db, &error));

    // Create:
    createDocument(db, "doc3", "foo", "bar3");

    // Delete:
    const CBLDocument* doc1 = CBLDatabase_GetDocument(db, "doc1"_sl, &error);
    REQUIRE(doc1);
    REQUIRE(CBLDatabase_DeleteDocument(db, doc1, &error));
    CBLDocument_Release(doc1);

    // Purge:
    const CBLDocument* doc2 = CBLDatabase_GetDocument(db, "doc2"_sl, &error);
    REQUIRE(doc2);
    REQUIRE(CBLDatabase_PurgeDocument(db, doc2, &error));
    CBLDocument_Release(doc2);

    // Abort Transaction:
    REQUIRE(CBLDatabase_EndTransaction(db, false, &error));
    
    CHECK(CBLDatabase_Count(db) == 2);
    doc1 = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(CBLDocument_ID(doc1) == "doc1"_sl);
    CBLDocument_Release(doc1);
    
    doc2 = CBLDatabase_GetMutableDocument(db, "doc2"_sl, &error);
    CHECK(CBLDocument_ID(doc2) == "doc2"_sl);
    CBLDocument_Release(doc2);
}


#pragma mark - LISTENERS:


static int dbListenerCalls = 0;
static int fooListenerCalls = 0;

static void dbListener(void *context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs) {
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 1);
    CHECK(slice(docIDs[0]) == "foo"_sl);
}

static void fooListener(void *context, const CBLDatabase *db, FLString docID) {
    ++fooListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(slice(docID) == "foo"_sl);
}


TEST_CASE_METHOD(DatabaseTest, "Database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener, this);
    auto docToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);

    // Create a doc, check that the listener was called:
    createDocument(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);

    CBLListener_Remove(token);
    CBLListener_Remove(docToken);

    // After being removed, the listener should not be called:
    dbListenerCalls = fooListenerCalls = 0;
    createDocument(db, "bar", "greeting", "yo.");
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
}


TEST_CASE_METHOD(DatabaseTest, "Remove Database Listener after releasing database") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener, this);
    auto docToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);

    // Create a doc, check that the listener was called:
    createDocument(db, "foo", "greeting", "Howdy!");
    CHECK(dbListenerCalls == 1);
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


static int notificationsReadyCalls = 0;

static void notificationsReady(void *context, CBLDatabase* db) {
    ++notificationsReadyCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
}

static void dbListener2(void *context, const CBLDatabase *db, unsigned nDocs, FLString *docIDs) {
    ++dbListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(nDocs == 2);
    CHECK(docIDs[0] == "foo"_sl);
    CHECK(docIDs[1] == "bar"_sl);
}

int barListenerCalls = 0;

static void barListener(void *context, const CBLDatabase *db, FLString docID) {
    ++barListenerCalls;
    auto test = (CBLTest*)context;
    CHECK(test->db == db);
    CHECK(docID == "bar"_sl);
}


TEST_CASE_METHOD(DatabaseTest, "Scheduled database notifications") {
    // Add a listener:
    dbListenerCalls = fooListenerCalls = barListenerCalls = 0;
    auto token = CBLDatabase_AddChangeListener(db, dbListener2, this);
    auto fooToken = CBLDatabase_AddDocumentChangeListener(db, "foo"_sl, fooListener, this);
    auto barToken = CBLDatabase_AddDocumentChangeListener(db, "bar"_sl, barListener, this);
    CBLDatabase_BufferNotifications(db, notificationsReady, this);

    // Create two docs; no listeners should be called yet:
    createDocument(db, "foo", "greeting", "Howdy!");
    CHECK(notificationsReadyCalls == 1);
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    createDocument(db, "bar", "greeting", "yo.");
    CHECK(notificationsReadyCalls == 1);
    CHECK(dbListenerCalls == 0);
    CHECK(fooListenerCalls == 0);
    CHECK(barListenerCalls == 0);

    // Now the listeners will be called:
    CBLDatabase_SendNotifications(db);
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    // There should be no more notifications:
    CBLDatabase_SendNotifications(db);
    CHECK(dbListenerCalls == 1);
    CHECK(fooListenerCalls == 1);
    CHECK(barListenerCalls == 1);

    CBLListener_Remove(token);
    CBLListener_Remove(fooToken);
    CBLListener_Remove(barToken);
}


TEST_CASE_METHOD(DatabaseTest, "Set blob in document", "[Blob]") {
    // Create and Save blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    
    // Set blob in document
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict docProps = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetBlob(docProps, FLSTR("blob"), blob);
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    CBLDocument_Release(doc);
    CBLBlob_Release(blob);
    
    // Get blob from the saved doc and check:
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    docProps = CBLDocument_MutableProperties(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(docProps, "blob"_sl));
    REQUIRE(blob2);
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    FLSliceResult_Release(content);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Set blob in document using indirect properties", "[Blob]") {
    // Create and Save blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    FLMutableDict copiedBlobProps = FLDict_MutableCopy(CBLBlob_Properties(blob), kFLDefaultCopy);
    CHECK(copiedBlobProps);
    
    // Set blob in document using indirect (copied) properties
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict docProps = CBLDocument_MutableProperties(doc);
    FLSlot_SetDict(FLMutableDict_Set(docProps, FLStr("blob")), copiedBlobProps);
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    CBLDocument_Release(doc);
    
    // Get blob from the saved doc and check:
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    docProps = CBLDocument_MutableProperties(doc);
    const CBLBlob* blob3 = FLValue_GetBlob(FLDict_Get(docProps, "blob"_sl));
    FLSliceResult content = CBLBlob_Content(blob3, &error);
    CHECK((slice)content == blobContent);
    FLSliceResult_Release(content);
    CBLDocument_Release(doc);
    
    // Release original blob and copied of blob properties:
    CBLBlob_Release(blob);
    FLMutableDict_Release(copiedBlobProps);
}


TEST_CASE_METHOD(DatabaseTest, "Save blob and set blob in document", "[Blob]") {
    // Create and Save blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    CHECK(CBLDatabase_SaveBlob(db, blob, &error));
    
    // Set blob in document
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict docProps = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetBlob(docProps, FLSTR("blob"), blob);
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    CBLDocument_Release(doc);
    CBLBlob_Release(blob);
    
    // Get blob from the saved doc and check:
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(doc);
    docProps = CBLDocument_MutableProperties(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(docProps, "blob"_sl));
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    FLSliceResult_Release(content);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save blob and set blob properties in document", "[Blob]") {
    // Create and Save blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    CHECK(CBLDatabase_SaveBlob(db, blob, &error));
    
    // Copy blob properties and release blob:
    FLMutableDict blobProps = FLDict_MutableCopy(CBLBlob_Properties(blob), kFLDefaultCopy);
    CBLBlob_Release(blob);
    
    // Use the blob properties in a document:
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict docProps = CBLDocument_MutableProperties(doc);
    FLSlot_SetDict(FLMutableDict_Set(docProps, FLStr("blob")), blobProps);
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    CBLDocument_Release(doc);
    FLMutableDict_Release(blobProps);
    
    // Get blob from the saved doc and check:
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(doc);
    docProps = CBLDocument_MutableProperties(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(docProps, "blob"_sl));
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    FLSliceResult_Release(content);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Save blob read from database", "[Blob]") {
    // Create blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    CHECK(CBLDatabase_SaveBlob(db, blob, &error));
    
    // Save doc with blob:
    CBLDocument* doc = CBLDocument_CreateWithID("doc1"_sl);
    FLMutableDict docProps = CBLDocument_MutableProperties(doc);
    FLSlot_SetDict(FLMutableDict_Set(docProps, FLStr("blob")), CBLBlob_Properties(blob));
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    CBLDocument_Release(doc);
    CBLBlob_Release(blob);
    
    // Get blob from the saved doc:
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    CHECK(doc);
    docProps = CBLDocument_MutableProperties(doc);
    const CBLBlob* blob2 = FLValue_GetBlob(FLDict_Get(docProps, "blob"_sl));
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    
    // Try to save blob from the saved doc:
    ExpectingExceptions x;
    CHECK(!CBLDatabase_SaveBlob(db, const_cast<CBLBlob*>(blob2), &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorUnsupported);
    FLSliceResult_Release(content);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(DatabaseTest, "Get non-existing blob", "[Blob]") {
    CBLError error;
    FLMutableDict blobProps = FLMutableDict_New();
    FLMutableDict_SetString(blobProps, kCBLTypeProperty, kCBLBlobType);
    FLMutableDict_SetString(blobProps, kCBLBlobDigestProperty, "sha1-VVVVVVVVVVVVVVVVVVVVVVVVVVU="_sl);
    ExpectingExceptions x;
    CHECK(!CBLDatabase_GetBlob(db, blobProps, &error));
    CHECK(error.code == 0);
    FLMutableDict_Release(blobProps);
}


TEST_CASE_METHOD(DatabaseTest, "Get blob using invalid properties", "[blob]") {
    CBLError error;
    FLMutableDict blobProps = FLMutableDict_New();
    FLMutableDict_SetString(blobProps, kCBLTypeProperty, kCBLBlobType);
    ExpectingExceptions x;
    CHECK(!CBLDatabase_GetBlob(db, blobProps, &error));
    CHECK(error.domain == kCBLDomain);
    CHECK(error.code == kCBLErrorInvalidParameter);
    FLMutableDict_Release(blobProps);
}


TEST_CASE_METHOD(DatabaseTest, "Get blob", "[Blob]") {
    // Create and Save blob:
    CBLError error;
    FLSlice blobContent = FLStr("I'm Blob.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, blobContent);
    CHECK(CBLDatabase_SaveBlob(db, blob, &error));
    
    // Copy blob properties and release blob:
    FLMutableDict blobProps = FLDict_MutableCopy(CBLBlob_Properties(blob), kFLDefaultCopy);
    CBLBlob_Release(blob);
    
    const CBLBlob* blob2 = CBLDatabase_GetBlob(db, blobProps, &error);
    FLSliceResult content = CBLBlob_Content(blob2, &error);
    CHECK((slice)content == blobContent);
    CBLBlob_Release(blob2);
    
    // Compact; blob should be deleted as it is not associated with any docs:
    CHECK(CBLDatabase_PerformMaintenance(db, kCBLMaintenanceTypeCompact, &error));
    ExpectingExceptions x;
    CHECK(!CBLDatabase_GetBlob(db, blobProps, &error));
    CHECK(error.code == 0);
}


#pragma mark - CLOSE AND DELETE DATABASE:


#ifdef COUCHBASE_ENTERPRISE

TEST_CASE_METHOD(DatabaseTest, "Close Database with Active Replicator", "[.CBL-3183]") {
    CBLError error;
    otherDB = CBLDatabase_Open(kOtherDBName, &kDatabaseConfiguration, &error);
    REQUIRE(otherDB);
    
    // Start Replicator:
    auto endpoint = CBLEndpoint_CreateWithLocalDB(otherDB);
    CBLReplicatorConfiguration config = {};
    config.database = db;
    config.endpoint = endpoint;
    config.continuous = true;
    auto repl = CBLReplicator_Create(&config, &error);
    REQUIRE(repl);
    CBLReplicator_Start(repl, false);
    
    // Wait util the replicator starts to run:
    int count = 0;
    while (CBLReplicator_Status(repl).activity == kCBLReplicatorStopped && count++ < 100) {
        this_thread::sleep_for(100ms);
    }
    CHECK(CBLReplicator_Status(repl).activity != kCBLReplicatorStopped);
    
    // Close Database:
    CHECK(CBLDatabase_Close(db, &error));
    
    // Check if the replicator is stopped:
    CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorStopped);
    
    CBLEndpoint_Free(endpoint);
    CBLReplicator_Release(repl);
    
    // For async clean up in Replicator:
    this_thread::sleep_for(200ms);
}

TEST_CASE_METHOD(DatabaseTest, "Delete Database with Active Replicator", "[.CBL-3183]") {
    CBLError error;
    otherDB = CBLDatabase_Open(kOtherDBName, &kDatabaseConfiguration, &error);
    REQUIRE(otherDB);
    
    // Start Replicator:
    auto endpoint = CBLEndpoint_CreateWithLocalDB(otherDB);
    CBLReplicatorConfiguration config = {};
    config.database = db;
    config.endpoint = endpoint;
    config.continuous = true;
    auto repl = CBLReplicator_Create(&config, &error);
    REQUIRE(repl);
    CBLReplicator_Start(repl, false);
    
    // Wait util the replicator starts to run:
    int count = 0;
    while (CBLReplicator_Status(repl).activity == kCBLReplicatorStopped && count++ < 100) {
        this_thread::sleep_for(100ms);
    }
    CHECK(CBLReplicator_Status(repl).activity != kCBLReplicatorStopped);
    
    // Delete Database:
    CHECK(CBLDatabase_Delete(db, &error));
    
    // Check if the replicator is stopped:
    CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorStopped);
    
    CBLEndpoint_Free(endpoint);
    CBLReplicator_Release(repl);
    
    // For async clean up in Replicator:
    this_thread::sleep_for(200ms);
}

#endif


TEST_CASE_METHOD(DatabaseTest, "Close Database with Active Live Query") {
    ImportJSONLines(GetTestFilePath("names_100.json"), db);
    
    CBLError error;
    auto query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                         "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                         nullptr, &error);
    REQUIRE(query);
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        auto newResults = CBLQuery_CopyCurrentResults(query, token, nullptr);
        CHECK(newResults);
        CBLResultSet_Release(newResults);
    }, this);
    REQUIRE(listenerToken);
    
    // Close database:
    CHECK(CBLDatabase_Close(db, &error));
    
    // Cleanup:
    CBLQuery_Release(query);
    CBLListener_Remove(listenerToken);
    
    // Sleeping to ensure async cleanup
    this_thread::sleep_for(200ms);
}


TEST_CASE_METHOD(DatabaseTest, "Delete Database with Active Live Query") {
    ImportJSONLines(GetTestFilePath("names_100.json"), db);
    
    CBLError error;
    auto query = CBLDatabase_CreateQuery(db, kCBLN1QLLanguage,
                                         "SELECT name FROM _ WHERE birthday like '1959-%' ORDER BY birthday"_sl,
                                         nullptr, &error);
    REQUIRE(query);
    auto listenerToken = CBLQuery_AddChangeListener(query, [](void *context, CBLQuery* query, CBLListenerToken* token) {
        auto newResults = CBLQuery_CopyCurrentResults(query, token, nullptr);
        CHECK(newResults);
        CBLResultSet_Release(newResults);
    }, this);
    REQUIRE(listenerToken);
    
    // Delete database:
    CHECK(CBLDatabase_Delete(db, &error));
    
    // Cleanup:
    CBLQuery_Release(query);
    CBLListener_Remove(listenerToken);
    
    // Sleeping to ensure async cleanup
    this_thread::sleep_for(200ms);
}
