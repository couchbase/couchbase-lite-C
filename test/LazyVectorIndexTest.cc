//
// LazyVectorIndexTest.cc
//
// Copyright © 2024 Couchbase. All rights reserved.
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

#include "VectorSearchTest.hh"
#include <algorithm>
#include <array>

#ifdef VECTOR_SEARCH_TEST_ENABLED

/**
 * Test Spec :
 * https://github.com/couchbaselabs/couchbase-lite-api/blob/master/spec/tests/T0002-Lazy-Vector-Index.md
 *
 * Note: 
 * - Test 1 (TestIsLazyDefaultValue) Not applicable for C
 * - Test 6 (TestGetIndexOnClosedDatabase) is done in "Close Database then Use Collection".
 * - Test 7 (testInvalidCollection) is done in "Delete Collection then Use Collection".
 * - Test 16 (TestIndexUpdaterArrayIterator) Not applicable for C (Not Implement Iterator in C, not the main purpose of the updater and hard to use)
 */

/**
 * 2. TestIsLazyAccessor
 *
 * Description
 * Test that isLazy getter/setter of the VectorIndexConfiguration work as expected.
 *
 * Steps
 * 1. Create a VectorIndexConfiguration object.
 *    - expression: word
 *    - dimensions: 300
 *    - centroids : 20
 * 2. Set isLazy to true
 * 3. Check that isLazy returns true.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIsLazyAccessor", "[VectorSearch][LazyVectorIndex]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    config.isLazy = true;
    CHECK(config.isLazy);
}

/**
 * 3. TestGetNonExistingIndex
 *
 * Description
 * Test that getting non-existing index object by name returning null.
 *
 * Steps
 * 1. Get the default collection from a test database.
 * 2. Get a QueryIndex object from the default collection with the name as
 *   "nonexistingindex".
 * 3. Check that the result is null.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestGetNonExistingIndex", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    auto index = CBLCollection_GetIndex(defaultCollection, "nonexistingindexsss"_sl, &error);
    CHECK(!index);
    CheckNoError(error);
}

/**
 * 4. TestGetExistingNonVectorIndex
 *
 * Description
 * Test that getting non-existing index object by name returning an index object correctly.
 *
 * Steps
 * 1. Get the default collection from a test database.
 * 2. Create a value index named "value_index" in the default collection
 *   with the expression as "value".
 * 3. Get a QueryIndex object from the default collection with the name as
 *   "value_index".
 * 4. Check that the result is not null.
 * 5. Check that the QueryIndex's name is "value_index".
 * 6. Check that the QueryIndex's collection is the same instance that
 *   is used for getting the QueryIndex object.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestGetExistingNonVectorIndex", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLValueIndexConfiguration config {};
    config.expressionLanguage = kCBLN1QLLanguage;
    config.expressions = "value"_sl;
    CHECK(CBLCollection_CreateValueIndex(defaultCollection, "value_index"_sl, config, &error));
    CheckNoError(error);
    
    auto index = CBLCollection_GetIndex(defaultCollection, "value_index"_sl, &error);
    CHECK(index);
    CHECK(CBLQueryIndex_Name(index) == "value_index"_sl);
    CHECK(CBLQueryIndex_Collection(index) == defaultCollection);
    CBLQueryIndex_Release(index);
}

/**
 * 5. TestGetExistingVectorIndex
 *
 * Description
 * Test that getting an existing index object by name returning an index object correctly.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "vector"
 *     - dimensions: 300
 *     - centroids : 8
 * 3. Get a QueryIndex object from the words collection with the name as
 *   "words_index".
 * 4. Check that the result is not null.
 * 5. Check that the QueryIndex's name is "words_index".
 * 6. Check that the QueryIndex's collection is the same instance that is used for
 *   getting the index.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestGetExistingVectorIndex", "[VectorSearch][LazyVectorIndex]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    // getWordsIndex() already checks index's name and collection
    auto index = getWordsIndex();
    CHECK(index);
    CBLQueryIndex_Release(index);
}

/**
 * 8. TestLazyVectorIndexNotAutoUpdatedChangedDocs
 *
 * Description
 * Test that the lazy index is lazy. The index will not be automatically
 * updated when the documents are created or updated.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Create an SQL++ query:
 *     - SELECT word
 *       FROM _default.words
 *       WHERE vector_match(words_index, < dinner vector >)
 * 4. Execute the query and check that 0 results are returned.
 * 5. Update the documents:
 *     - Create _default.words.word301 with the content from _default.extwords.word1
 *     - Update _default.words.word1 with the content from _default.extwords.word3
 * 6. Execute the same query and check that 0 results are returned.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestLazyVectorIndexNotAutoUpdatedChangedDocs", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8, true };
    createWordsIndex(config);
    
    auto results = executeWordsQuery();
    CHECK(CountResults(results) == 0);
    CBLResultSet_Release(results);
    
    auto doc1 = CBLCollection_GetDocument(extwordsCollection, "word1"_sl, &error);
    REQUIRE(doc1);
    copyDocument(wordsCollection, "word301", doc1);
    
    auto doc2 = CBLCollection_GetDocument(extwordsCollection, "word3"_sl, &error);
    REQUIRE(doc2);
    copyDocument(wordsCollection, "word1", doc2);
    
    results = executeWordsQuery();
    CHECK(CountResults(results) == 0);
    CBLResultSet_Release(results);
    
    CBLDocument_Release(doc1);
    CBLDocument_Release(doc2);
}

/**
 * 9. TestLazyVectorIndexAutoUpdateDeletedDocs
 *
 * Description
 * Test that when the lazy vector index automatically update when documents are
 * deleted.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Call beginUpdate() with limit 1 to get an IndexUpdater object.
 * 4. Check that the IndexUpdater is not null and IndexUpdater.count = 1.
 * 5. With the IndexUpdater object:
 *    - Get the word string from the IndexUpdater.
 *    - Query the vector by word from the _default.words collection.
 *    - Convert the vector result which is an array object to a platform's float array.
 *    - Call setVector() with the platform's float array at the index.
 *    - Call finish()
 * 6. Create an SQL++ query:
 *    - SELECT word
 *      FROM _default.words
 *      WHERE vector_match(words_index, < dinner vector >) LIMIT 300
 * 7. Execute the query and check that 1 results are returned.
 * 8. Check that the word gotten from the query result is the same as the word in Step 5.
 * 9. Delete _default.words.word1 doc.
 * 10. Execute the same query as Step again and check that 0 results are returned.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestLazyVectorIndexAutoUpdateDeletedDocs", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 1, &error);
    CHECK(updater);
    CHECK(CBLIndexUpdater_Count(updater) == 1);
    
    auto value = CBLIndexUpdater_Value(updater, 0);
    auto word = FLValue_AsString(value);
    CHECK(word);
    
    // For checking query result later:
    auto wordStr = slice(word).asString();
    
    updateWordsIndexWithUpdater(updater);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
    
    // Query:
    auto results = executeWordsQuery(300);
    
    // Check results:
    auto map = mapWordResults(results);
    CHECK(map.size() == 1);
    auto it = map.begin();
    CHECK(wordStr == it->second);
    
    // Delete doc:
    auto docID = it->first;
    REQUIRE(CBLCollection_DeleteDocumentByID(wordsCollection, slice(docID), &error));
    
    // Query Again:
    CBLResultSet_Release(results);
    results = executeWordsQuery(300);
    
    // Check results:
    CHECK(CountResults(results) == 0);
    CBLResultSet_Release(results);
}

/**
 * 10. TestLazyVectorIndexAutoUpdatePurgedDocs
 *
 * Description
 * Test that when the lazy vector index automatically update when documents are
 * purged.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Call beginUpdate() with limit 1 to get an IndexUpdater object.
 * 4. Check that the IndexUpdater is not null and IndexUpdater.count = 1.
 * 5. With the IndexUpdater object:
 *    - Get the word string from the IndexUpdater.
 *    - Query the vector by word from the _default.words collection.
 *    - Convert the vector result which is an array object to a platform's float array.
 *    - Call setVector() with the platform's float array at the index.
 * 6. With the IndexUpdater object, call finish()
 * 7. Create an SQL++ query:
 *    - SELECT word
 *      FROM _default.words
 *      WHERE vector_match(words_index, < dinner vector >) LIMIT 300
 * 8. Execute the query and check that 1 results are returned.
 * 9. Check that the word gotten from the query result is the same as the word in Step 5.
 * 10. Purge _default.words.word1 doc.
 * 11. Execute the same query as Step again and check that 0 results are returned.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestLazyVectorIndexAutoUpdatePurgedDocs", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 1, &error);
    CHECK(updater);
    CHECK(CBLIndexUpdater_Count(updater) == 1);
    
    auto value = CBLIndexUpdater_Value(updater, 0);
    auto word = FLValue_AsString(value);
    CHECK(word);
    
    // For checking query result later:
    auto wordStr = slice(word).asString();
    
    updateWordsIndexWithUpdater(updater);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
    
    // Query:
    auto results = executeWordsQuery(300);
    
    // Check results:
    auto map = mapWordResults(results);
    CHECK(map.size() == 1);
    auto it = map.begin();
    CHECK(wordStr == it->second);
    
    // Purge doc:
    auto docID = it->first;
    REQUIRE(CBLCollection_PurgeDocumentByID(wordsCollection, slice(docID), &error));
    
    // Query Again:
    CBLResultSet_Release(results);
    results = executeWordsQuery(300);
    
    // Check results:
    CHECK(CountResults(results) == 0);
    CBLResultSet_Release(results);
}

/**
 * 11. TestIndexUpdaterBeginUpdateOnNonVectorIndex
 *
 * Description
 * Test that a CouchbaseLiteException is thrown when calling beginUpdate on
 * a non vector index.
 *
 * Steps
 * 1. Get the default collection from a test database.
 * 2. Create a value index named "value_index" in the default collection with the
 *   expression as "value".
 * 3. Get a QueryIndex object from the default collection with the name as
 *   "value_index".
 * 4. Call beginUpdate() with limit 10 on the QueryIndex object.
 * 5. Check that a CouchbaseLiteException with the code Unsupported is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterBeginUpdateOnNonVectorIndex", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLValueIndexConfiguration config {};
    config.expressionLanguage = kCBLN1QLLanguage;
    config.expressions = "value"_sl;
    CHECK(CBLCollection_CreateValueIndex(defaultCollection, "value_index"_sl, config, &error));
    CheckNoError(error);
    
    auto index = CBLCollection_GetIndex(defaultCollection, "value_index"_sl, &error);
    CHECK(index);
    
    ExpectingExceptions x {};
    auto updater = CBLQueryIndex_BeginUpdate(index, 10, &error);
    CHECK(!updater);
    CheckError(error, kCBLErrorUnsupported);
    
    CBLQueryIndex_Release(index);
}

/**
 * 12. TestIndexUpdaterBeginUpdateOnNonLazyVectorIndex
 *
 * Description
 * Test that a CouchbaseLiteException is thrown when calling beginUpdate
 * on a non lazy vector index.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 * 3. Get a QueryIndex object from the words collection with the name as
 *   "words_index".
 * 4. Call beginUpdate() with limit 10 on the QueryIndex object.
 * 5. Check that a CouchbaseLiteException with the code Unsupported is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterBeginUpdateOnNonLazyVectorIndex", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8 };
    createWordsIndex(config);
    
    auto index = getWordsIndex();
    
    ExpectingExceptions x {};
    auto updater = CBLQueryIndex_BeginUpdate(index, 10, &error);
    CHECK(!updater);
    CheckError(error, kCBLErrorUnsupported);
    
    CBLQueryIndex_Release(index);
}

/**
 * 13. TestIndexUpdaterBeginUpdateWithZeroLimit
 *
 * Description
 * Test that an InvalidArgument exception is returned when calling beginUpdate
 * with zero limit.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Get a QueryIndex object from the words collec
 *    "words_index".
 * 4. Call beginUpdate() with limit 0 on the QueryIndex object.
 * 5. Check that an InvalidArgumentException is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterBeginUpdateWithZeroLimit", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    auto index = getWordsIndex();
    
    ExpectingExceptions x {};
    auto updater = CBLQueryIndex_BeginUpdate(index, 0, &error);
    CHECK(!updater);
    CheckError(error, kCBLErrorInvalidParameter);
    
    CBLQueryIndex_Release(index);
}

/**
 * 14. TestIndexUpdaterBeginUpdateOnLazyVectorIndex
 *
 * Description
 * Test that calling beginUpdate on a lazy vector index returns an IndexUpdater.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Get a QueryIndex object from the words with the name as "words_index".
 * 4. Call beginUpdate() with limit 0 on the QueryIndex object.
 * 5. Check that the returned IndexUpdater is not null.
 * 6. Check that the IndexUpdater.count is 10.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterBeginUpdateOnLazyVectorIndex", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    auto index = getWordsIndex();
    
    ExpectingExceptions x {};
    auto updater = CBLQueryIndex_BeginUpdate(index, 10, &error);
    CHECK(updater);
    CheckNoError(error);
    CHECK(CBLIndexUpdater_Count(updater) == 10);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
}

/**
 * 15. TestIndexUpdaterGettingValues
 *
 * Description
 * Test all type getters and toArary() from the Array interface. The test
 * may be divided this test into multiple tests per type getter as appropriate.
 *
 * Steps
 * 1. Get the default collection from a test database.
 * 2. Create the followings documents:
 *     - doc-0 : { "value": "a string" }
 *     - doc-1 : { "value": 100 }
 *     - doc-2 : { "value": 20.8 }
 *     - doc-3 : { "value": true }
 *     - doc-4 : { "value": false }
 *     - doc-5 : { "value": Date("2024-05-10T00:00:00.000Z") }
 *     - doc-6 : { "value": Blob(Data("I'm Bob")) }
 *     - doc-7 : { "value": {"name": "Bob"} }
 *     - doc-8 : { "value": ["one", "two", "three"] }
 *     - doc-9 : { "value": null }
 * 3. Create a vector index named "vector_index" in the default collection.
 *     - expression: "value"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 4. Get a QueryIndex object from the default collection with the name as
 *    "vector_index".
 * 5. Call beginUpdate() with limit 10 to get an IndexUpdater object.
 * 6. Check that the IndexUpdater.count is 10.
 * 7. Get string value from each index and check the followings:
 *     - getString(0) : value == "a string"
 *     - getString(1) : value == null
 *     - getString(2) : value == null
 *     - getString(3) : value == null
 *     - getString(4) : value == null
 *     - getString(5) : value == "2024-05-10T00:00:00.000Z"
 *     - getString(6) : value == null
 *     - getString(7) : value == null
 *     - getString(8) : value == null
 *     - getString(9) : value == null
 * 8. Get integer value from each index and check the followings:
 *     - getInt(0) : value == 0
 *     - getInt(1) : value == 100
 *     - getInt(2) : value == 20
 *     - getInt(3) : value == 1
 *     - getInt(4) : value == 0
 *     - getInt(5) : value == 0
 *     - getInt(6) : value == 0
 *     - getInt(7) : value == 0
 *     - getInt(8) : value == 0
 *     - getInt(9) : value == 0
 * 9. Get float value from each index and check the followings:
 *     - getFloat(0) : value == 0.0
 *     - getFloat(1) : value == 100.0
 *     - getFloat(2) : value == 20.8
 *     - getFloat(3) : value == 1.0
 *     - getFloat(4) : value == 0.0
 *     - getFloat(5) : value == 0.0
 *     - getFloat(6) : value == 0.0
 *     - getFloat(7) : value == 0.0
 *     - getFloat(8) : value == 0.0
 *     - getFloat(9) : value == 0.0
 * 10. Get double value from each index and check the followings:
 *     - getDouble(0) : value == 0.0
 *     - getDouble(1) : value == 100.0
 *     - getDouble(2) : value == 20.8
 *     - getDouble(3) : value == 1.0
 *     - getDouble(4) : value == 0.0
 *     - getDouble(5) : value == 0.0
 *     - getDouble(6) : value == 0.0
 *     - getDouble(7) : value == 0.0
 *     - getDouble(8) : value == 0.0
 *     - getDouble(9) : value == 0.0
 * 11. Get boolean value from each index and check the followings:
 *     - getBoolean(0) : value == true
 *     - getBoolean(1) : value == true
 *     - getBoolean(2) : value == true
 *     - getBoolean(3) : value == true
 *     - getBoolean(4) : value == false
 *     - getBoolean(5) : value == true
 *     - getBoolean(6) : value == true
 *     - getBoolean(7) : value == true
 *     - getBoolean(8) : value == true
 *     - getBoolean(9) : value == false
 * 12. Get date value from each index and check the followings:
 *     - getDate(0) : value == "2024-05-10T00:00:00.000Z"
 *     - getDate(1) : value == null
 *     - getDate(2) : value == null
 *     - getDate(3) : value == null
 *     - getDate(4) : value == null
 *     - getDate(5) : value == Date("2024-05-10T00:00:00.000Z")
 *     - getDate(6) : value == null
 *     - getDate(7) : value == null
 *     - getDate(8) : value == null
 *     - getDate(9) : value == null
 * 13. Get blob value from each index and check the followings:
 *     - getBlob(0) : value == null
 *     - getBlob(1) : value == null
 *     - getBlob(2) : value == null
 *     - getBlob(3) : value == null
 *     - getBlob(4) : value == null
 *     - getBlob(5) : value == null
 *     - getBlob(6) : value == Blob(Data("I'm Bob"))
 *     - getBlob(7) : value == null
 *     - getBlob(8) : value == null
 *     - getBlob(9) : value == null
 * 14. Get dictionary object from each index and check the followings:
 *     - getDictionary(0) : value == null
 *     - getDictionary(1) : value == null
 *     - getDictionary(2) : value == null
 *     - getDictionary(3) : value == null
 *     - getDictionary(4) : value == null
 *     - getDictionary(5) : value == null
 *     - getDictionary(6) : value == null
 *     - getDictionary(7) : value == Dictionary({"name": "Bob"})
 *     - getDictionary(8) : value == null
 *     - getDictionary(9) : value == null
 * 15. Get array object from each index and check the followings:
 *     - getArray(0) : value == null
 *     - getArray(1) : value == null
 *     - getArray(2) : value == null
 *     - getArray(3) : value == null
 *     - getArray(4) : value == null
 *     - getArray(5) : value == null
 *     - getArray(6) : value == null
 *     - getArray(7) : value == null
 *     - getArray(8) : value == Array(["one", "two", "three"])
 *     - getArray(9) : value == null
 * 16. Get value from each index and check the followings:
 *     - getValue(0) : value == "a string"
 *     - getValue(1) : value == PlatformNumber(100)
 *     - getValue(2) : value == PlatformNumber(20.8)
 *     - getValue(3) : value == PlatformBoolean(true)
 *     - getValue(4) : value == PlatformBoolean(false)
 *     - getValue(5) : value == Date("2024-05-10T00:00:00.000Z")
 *     - getValue(6) : value == Blob(Data("I'm Bob"))
 *     - getValue(7) : value == Dictionary({"name": "Bob"})
 *     - getValue(8) : value == Array(["one", "two", "three"])
 *     - getValue(9) : value == null
 * 17. Get IndexUodater values as a platform array by calling toArray() and check
 *     that the array contains all values as expected.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterGettingValues", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    createDocWithJSON(defaultCollection, "doc-0", R"({"value":"a string"})");
    createDocWithJSON(defaultCollection, "doc-1", R"({"value":100})");
    createDocWithJSON(defaultCollection, "doc-2", R"({"value":20.8})");
    createDocWithJSON(defaultCollection, "doc-3", R"({"value":true})");
    createDocWithJSON(defaultCollection, "doc-4", R"({"value":false})");
    
    CBLDocument* doc = CBLDocument_CreateWithID("doc-5"_sl);
    FLMutableDict docProps = CBLDocument_MutableProperties(doc);
    auto blob1 = CBLBlob_CreateWithData("text/plain"_sl, "I'm Bob"_sl);
    FLMutableDict_SetBlob(docProps, "value"_sl, blob1);
    CHECK(CBLCollection_SaveDocument(defaultCollection, doc, &error));
    CheckNoError(error);
    CBLDocument_Release(doc);
    
    createDocWithJSON(defaultCollection, "doc-6", R"({"value":{"name":"Bob"}})");
    createDocWithJSON(defaultCollection, "doc-7", R"({"value":["one","two","three"]})");
    createDocWithJSON(defaultCollection, "doc-8", R"({"value":null})");
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "value"_sl, 300, 8, true };
    createVectorIndex(defaultCollection, "vector_index"_sl, config);
    
    auto index = CBLCollection_GetIndex(defaultCollection, "vector_index"_sl, &error);
    CHECK(index);
    CheckNoError(error);
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 9, &error);
    CHECK(updater);
    CheckNoError(error);
    CHECK(CBLIndexUpdater_Count(updater) == 9);
    
    // NOTE: CBL-C CBLIndexUpdater returns Fleece's FLValue when getting a value.
    // Thus, checking the correctness of the returned value for each index is sufficient.
    
    // String:
    FLValue val0 = CBLIndexUpdater_Value(updater, 0);
    CHECK(FLValue_AsString(val0) == "a string"_sl);
    
    // Integer:
    FLValue val1 = CBLIndexUpdater_Value(updater, 1);
    CHECK(FLValue_AsInt(val1) == 100);
    
    // Double:
    FLValue val2 = CBLIndexUpdater_Value(updater, 2);
    CHECK(FLValue_AsDouble(val2) == 20.8);
    
    // Boolean:
    FLValue val3 = CBLIndexUpdater_Value(updater, 3);
    CHECK(FLValue_AsBool(val3) == true);
    
    FLValue val4 = CBLIndexUpdater_Value(updater, 4);
    CHECK(FLValue_AsBool(val4) == false);
    
    // Blob:
    FLValue val5 = CBLIndexUpdater_Value(updater, 5);
    auto blob2 = FLValue_GetBlob(val5);
    CHECK(blob2);
    CHECK(!slice(CBLBlob_Digest(blob2)).empty());
    CHECK(CBLBlob_Digest(blob2) == CBLBlob_Digest(blob1));
    alloc_slice content = CBLBlob_Content(blob2, &error);
    CHECK(content == "I'm Bob");
    CBLBlob_Release(blob1);
    
    // Dict:
    FLValue val6 = CBLIndexUpdater_Value(updater, 6);
    auto dict = FLValue_AsDict(val6);
    CHECK(Dict(dict).toJSONString() == R"({"name":"Bob"})");
    
    // Array:
    FLValue val7 = CBLIndexUpdater_Value(updater, 7);
    auto array = FLValue_AsArray(val7);
    CHECK(Array(array).toJSONString() == R"(["one","two","three"])");
    
    // Null:
    FLValue val8 = CBLIndexUpdater_Value(updater, 8);
    CHECK(FLValue_GetType(val8) == kFLNull);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
}

/**
 * 17. TestIndexUpdaterSetFloatArrayVectors
 *
 * Description
 * Test that setting float array vectors works as expected.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Get a QueryIndex object from the words with the name as "words_index".
 * 4. Call beginUpdate() with limit 10 to get an IndexUpdater object.
 * 5. With the IndexUpdater object, for each index from 0 to 9.
 *     - Get the word string from the IndexUpdater and store the word string in a set for verifying
 *        the vector search result.
 *     - Query the vector by word from the _default.words collection.
 *     - Convert the vector result which is an array object to a platform's float array.
 *     - Call setVector() with the platform's float array at the index.
 * 6. With the IndexUpdater object, call finish()
 * 7. Execute a vector search query.
 *     - SELECT word
 *       FROM _default.words
 *       WHERE vector_match(words_index, < dinner vector >) LIMIT 300
 * 8. Check that there are 10 words returned.
 * 9. Check that the word is in the word set from the step 5.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterSetFloatArrayVectors", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 10, &error);
    CHECK(updater);
    
    vector<string> updatedWords {};
    updateWordsIndexWithUpdater(updater, true, &updatedWords);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
    
    // Query:
    auto results = executeWordsQuery(300);
    auto words = wordResults(results);
    CHECK(words.size() == 10);
    for (auto& word : updatedWords) {
        CHECK(find(words.begin(), words.end(), word) != words.end());
    }
    CBLResultSet_Release(results);
}

/**
 * 20. TestIndexUpdaterSetInvalidVectorDimensions
 *
 * Description
 * Test thta the vector with the invalid dimenions different from the dimensions
 * set to the configuration will not be included in the index.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Get a QueryIndex object from the words with the name as "words_index".
 * 4. Call beginUpdate() with limit 1 to get an IndexUpdater object.
 * 5. With the IndexUpdater object, call setVector() with a float array as [1.0]
 * 6. With the IndexUpdater object, call finish().
 * 7. Execute a vector search query.
 *     - SELECT word
 *       FROM _default.words
 *       WHERE vector_match(words_index, < dinner vector >) LIMIT 300
 * 8. Check that there are 0 words returned.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterSetInvalidVectorDimensions", "[.CBL-5814]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 1, &error);
    CHECK(updater);
    
    float vector[1] = {1.0};
    CHECK(CBLIndexUpdater_SetVector(updater, 0, vector, 1, &error));
    CheckError(error, kCBLErrorInvalidParameter);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
}

/**
 * 21. TestIndexUpdaterSkipVectors
 *
 * Description
 * Test that skipping vectors works as expected.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Get a QueryIndex object from the words with the name as "words_index".
 * 4. Call beginUpdate() with limit 10 to get an IndexUpdater object.
 * 5. With the IndexUpdater object, for each index from 0 - 9.
 *     - Get the word string from the IndexUpdater.
 *     - If index % 2 == 0,
 *         - Store the word string in a skipped word set for verifying the
 *           skipped words later.
 *         - Call skipVector at the index.
 *     - If index % 2 != 0,
 *         - Store the word string in a indexed word set for verifying the
 *           vector search result.
 *         - Query the vector by word from the _default.words collection.
 *         - Convert the vector result which is an array object to a platform's float array.
 *         - Call setVector() with the platform's float array at the index.
 * 6. With the IndexUpdater object, call finish()
 * 7. Execute a vector search query.
 *     - SELECT word
 *       FROM _default.words
 *       WHERE vector_match(words_index, < dinner vector >) LIMIT 300
 * 8. Check that there are 5 words returned.
 * 9. Check that the word is in the indexed word set from the step 5.
 * 10. Call beginUpdate() with limit 5 to get an IndexUpdater object.
 * 11. With the IndexUpdater object, for each index from 0 - 4.
 *     - Get the word string from the dictionary for the key named "word".
 *     - Check that the word is in the skipped word set from the step 5.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterSkipVectors", "[.CBL-5842]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 10, &error);
    CHECK(updater);
    
    vector<string> updatedWords {};
    vector<string> skippedWords {};
    updateWordsIndexWithUpdater(updater, true, &updatedWords, &skippedWords, [](int i) -> bool {
        return (i % 2 != 0);
    });
    CHECK(updatedWords.size() == 5);
    CHECK(skippedWords.size() == 5);
    
    CBLIndexUpdater_Release(updater);
    
    // Query:
    auto results = executeWordsQuery(300);
    auto words = wordResults(results);
    CHECK(words.size() == 5);
    for (auto& word : updatedWords) {
        CHECK(find(words.begin(), words.end(), word) != words.end());
    }
    CBLResultSet_Release(results);
    
    // Update index for the skipped words:
    updater = CBLQueryIndex_BeginUpdate(index, 5, &error);
    CHECK(updater);
    
    updatedWords.clear();
    updateWordsIndexWithUpdater(updater, true, &updatedWords);
    
    // Check:
    CHECK(updatedWords.size() == 5);
    CHECK(updatedWords == skippedWords);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
}

/**
 * 22. TestIndexUpdaterFinishWithIncompletedUpdate
 *
 * Description
 * Test that a CouchbaseLiteException is thrown when calling finish() on
 * an IndexUpdater that has incomplete updated.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Get a QueryIndex object from the words with the name as "words_index".
 * 4. Call beginUpdate() with limit 2 to get an IndexUpdater object.
 * 5. With the IndexUpdater object, call finish().
 * 6. Check that a CouchbaseLiteException with code UnsupportedOperation is thrown.
 * 7. For the index 0,
 *     - Get the word string from the IndexUpdater.
 *     - Query the vector by word from the _default.words collection.
 *     - Convert the vector result which is an array object to a platform's float array.
 *     - Call setVector() with the platform's float array at the index.
 * 8. With the IndexUpdater object, call finish().
 * 9. Check that a CouchbaseLiteException with code UnsupportedOperation is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterFinishWithIncompletedUpdate", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 2, &error);
    CHECK(updater);
    
    auto value = CBLIndexUpdater_Value(updater, 0);
    auto word = FLValue_AsString(value);
    CHECK(word);
    
    auto vector = vectorForWord(word);
    CHECK(!vector.empty());
    CHECK(CBLIndexUpdater_SetVector(updater, 0, vector.data(), vector.size(), &error));
    CheckNoError(error);
    
    ExpectingExceptions x {};
    CHECK(!CBLIndexUpdater_Finish(updater, &error));
    CheckError(error, kCBLErrorUnsupported);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
}

/**
 * 23. TestIndexUpdaterCaughtUp
 *
 * Description
 * Test that when the lazy vector index is caught up, calling beginUpdate() to
 * get an IndexUpdater will return null.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Call beginUpdate() with limit 100 to get an IndexUpdater object.
 *     - Get the word string from the IndexUpdater.
 *     - Query the vector by word from the _default.words collection.
 *     - Convert the vector result which is an array object to a platform's float array.
 *     - Call setVector() with the platform's float array at the index.
 * 4. Repeat Step 3 two more times.
 * 5. Call beginUpdate() with limit 100 to get an IndexUpdater object.
 * 6. Check that the returned IndexUpdater is null.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterCaughtUp", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    for (int i = 0; i < 3; i++) {
        auto updater = CBLQueryIndex_BeginUpdate(index, 100, &error);
        CHECK(updater);
        
        vector<string> updatedWords {};
        updateWordsIndexWithUpdater(updater, true, &updatedWords);
        
        CBLIndexUpdater_Release(updater);
    }
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 100, &error);
    CHECK(!updater);
    CheckNoError(error);
    
    CBLQueryIndex_Release(index);
}

/**
 * 24. TestNonFinishedIndexUpdaterNotUpdateIndex
 *
 * Description
 * Test that the index updater can be released without calling finish(),
 * and the released non-finished index updater doesn't update the index.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Get a QueryIndex object from the words with the name as "words_index".
 * 4. Call beginUpdate() with limit 10 to get an IndexUpdater object.
 * 5. With the IndexUpdater object, for each index from 0 - 9.
 *     - Get the word string from the IndexUpdater.
 *     - Query the vector by word from the _default.words collection.
 *     - Convert the vector result which is an array object to a platform's float array.
 *     - Call setVector() with the platform's float array at the index.
 * 6. Release or close the index updater object.
 * 7. Execute a vector search query.
 *     - SELECT word
 *       FROM _default.words
 *       WHERE vector_match(words_index, < dinner vector >) LIMIT 300
 * 8. Check that there are 0 words returned.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestNonFinishedIndexUpdaterNotUpdateIndex", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 10, &error);
    CHECK(updater);
    
    vector<string> updatedWords {};
    updateWordsIndexWithUpdater(updater, false, &updatedWords);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
    
    // Query:
    auto results = executeWordsQuery(300);
    CHECK(CountResults(results) == 0);
    CBLResultSet_Release(results);
}

/**
 * 25. TestIndexUpdaterIndexOutOfBounds
 *
 * Description
 * Test that when using getter, setter, and skip function with the index that
 * is out of bounds, an IndexOutOfBounds or InvalidArgument exception
 * is throws.
 *
 * Steps
 * 1. Get the default collection from a test database.
 * 2. Create the followings documents:
 *     - doc-0 : { "value": "a string" }
 * 3. Create a vector index named "vector_index" in the default collection.
 *     - expression: "value"
 *     - dimensions: 3
 *     - centroids : 8
 *     - isLazy : true
 * 4. Get a QueryIndex object from the default collection with the name as
 *    "vector_index".
 * 5. Call beginUpdate() with limit 10 to get an IndexUpdater object.
 * 6. Check that the IndexUpdater.count is 1.
 * 7. Call each getter function with index = -1 and check that
 *    an IndexOutOfBounds or InvalidArgument exception is thrown.
 * 8. Call each getter function with index = 1 and check that
 *    an IndexOutOfBounds or InvalidArgument exception is thrown.
 * 9. Call setVector() function with a vector = [1.0, 2.0, 3.0] and index = -1 and check that
 *    an IndexOutOfBounds or InvalidArgument exception is thrown.
 * 10. Call setVector() function with a vector = [1.0, 2.0, 3.0] and index = 1 and check that
 *    an IndexOutOfBounds or InvalidArgument exception is thrown.
 * 9. Call skipVector() function with index = -1 and check that
 *    an IndexOutOfBounds or InvalidArgument exception is thrown.
 * 10. Call skipVector() function with index = 1 and check that
 *    an IndexOutOfBounds or InvalidArgument exception is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterIndexOutOfBounds", "[VectorSearch][LazyVectorIndex]") {
    CBLError error {};
    
    createDocWithPair(defaultCollection, "doc-0"_sl, "value"_sl, "a string"_sl);
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "value"_sl, 3, 8, true };
    createVectorIndex(defaultCollection, "vector_index"_sl, config);
    
    auto index = CBLCollection_GetIndex(defaultCollection, "vector_index"_sl, &error);
    CHECK(index);
    CheckNoError(error);
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 10, &error);
    CHECK(updater);
    CheckNoError(error);
    CHECK(CBLIndexUpdater_Count(updater) == 1);
    
    array<int, 2> indices { -1, 1 };
    for (auto i : indices) {
        {
            // This is in line with FLArray, returns null when index is out-of-bound.
            ExpectingExceptions x {};
            auto value = CBLIndexUpdater_Value(updater, i);
            CHECK(!value);
        }
        
        {
            ExpectingExceptions x {};
            float vector[] = { 1.0, 2.0, 3.0 };
            CHECK(!CBLIndexUpdater_SetVector(updater, i, vector, 3, &error));
            CheckError(error, kCBLErrorInvalidParameter);
        }
        
        {
            error = {};
            ExpectingExceptions x {};
            CHECK(!CBLIndexUpdater_SkipVector(updater, i, &error));
            CheckError(error, kCBLErrorInvalidParameter);
        }
    }
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
}

/**
 * 26. TestIndexUpdaterCallFinishTwice
 *
 * Description
 * Test that when calling IndexUpdater's finish() after it was finished,
 * a CuchbaseLiteException is thrown.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in the _default.words collection.
 *     - expression: "word"
 *     - dimensions: 300
 *     - centroids : 8
 *     - isLazy : true
 * 3. Call beginUpdate() with limit 1 to get an IndexUpdater object.
 *     - Get the word string from the IndexUpdater.
 *     - Query the vector by word from the _default.words collection.
 *     - Convert the vector result which is an array object to a platform's float array.
 *     - Call setVector() with the platform's float array at the index..
 * 8. Call finish() and check that the finish() is successfully called.
 * 9. Call finish() again and check that a CouchbaseLiteException with the code Unsupported is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexUpdaterCallFinishTwice", "[.CBL-5843]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "word"_sl, 300, 8, true };
    createWordsIndex(config);
    
    // Update Index:
    auto index = getWordsIndex();
    
    auto updater = CBLQueryIndex_BeginUpdate(index, 1, &error);
    CHECK(updater);
    
    // This will call finish:
    updateWordsIndexWithUpdater(updater);
    
    // Call finish again:
    CHECK(!CBLIndexUpdater_Finish(updater, &error));
    CheckError(error, kCBLErrorUnsupported);
    
    CBLIndexUpdater_Release(updater);
    CBLQueryIndex_Release(index);
}

#endif
