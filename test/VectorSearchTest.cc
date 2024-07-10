//
// VectorSearchTest.cc
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

#ifdef COUCHBASE_ENTERPRISE

std::vector<string> VectorSearchTest::sVectorSearchTestLogs {};

#ifdef VECTOR_SEARCH_TEST_ENABLED

/**
 Test Spec :
 https://github.com/couchbaselabs/couchbase-lite-api/blob/master/spec/tests/T0001-Vector-Search.md
 
 NOTE: #1 TestVectorIndexConfigurationDefaultValue and #2 TestVectorIndexConfigurationSettersAndGetters does't applicable for CBL-C as
 CBLVectorIndexConfiguration is just a C struct and C struct doesn't have default value other than 0 or NULL.
 */

/**
 * 3. TestDimensionsValidation
 * Description
 *     Test that the dimensions are validated correctly. The invalid argument exception
 *     should be thrown when creating vector index configuration objects with invalid
 *     dimensions.
 * Steps
 *     1. Create a VectorIndexConfiguration object.
 *         - expression: "vector"
 *         - dimensions: 2 and 4096
 *         - centroids: 8
 *     2. Check that the config can be created without an error thrown.
 *     3. Use the config to create the index and check that the index
 *       can be created successfully.
 *     4. Change the dimensions to 1 and 2049.
 *     5. Check that an invalid argument exception is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestDimensionsValidation", "[VectorSearch]") {
    CBLError error {};
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl };
    config.centroids = 8;
    
    config.dimensions = 2;
    CHECK(CBLCollection_CreateVectorIndex(wordsCollection, "words_index1"_sl, config, &error));
    
    config.dimensions = 4096;
    CHECK(CBLCollection_CreateVectorIndex(wordsCollection, "words_index2"_sl, config, &error));
    
    ExpectingExceptions x;
    config.dimensions = 1;
    CHECK_FALSE(CBLCollection_CreateVectorIndex(wordsCollection, "words_index2"_sl, config, &error));
    CheckError(error, kCBLErrorInvalidParameter, kCBLDomain);
    
    error = {};
    config.dimensions = 4097;
    CHECK_FALSE(CBLCollection_CreateVectorIndex(wordsCollection, "words_index2"_sl, config, &error));
    CheckError(error, kCBLErrorInvalidParameter, kCBLDomain);
}

/**
 * 4. TestCentroidsValidation
 * Description
 *     Test that the centroids value is validated correctly. The invalid argument
 *     exception should be thrown when creating vector index configuration objects with
 *     invalid centroids..
 * Steps
 *     1. Create a VectorIndexConfiguration object.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 1 and 64000
 *     2. Check that the config can be created without an error thrown.
 *     3. Use the config to create the index and check that the index
 *        can be created successfully.
 *     4. Change the centroids to 0 and 64001.
 *     5. Check that an invalid argument exception is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCentroidsValidation", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300 };
    
    CBLError error {};
    config.centroids = 1;
    CHECK(CBLCollection_CreateVectorIndex(wordsCollection, "words_index1"_sl, config, &error));
    
    config.centroids = 6400;
    CHECK(CBLCollection_CreateVectorIndex(wordsCollection, "words_index2"_sl, config, &error));
    
    ExpectingExceptions x;
    
    config.centroids = 0;
    CHECK_FALSE(CBLCollection_CreateVectorIndex(wordsCollection, "words_index2"_sl, config, &error));
    CheckError(error, kCBLErrorInvalidParameter, kCBLDomain);
    
    error = {};
    config.centroids = 64001;
    CHECK_FALSE(CBLCollection_CreateVectorIndex(wordsCollection, "words_index2"_sl, config, &error));
    CheckError(error, kCBLErrorInvalidParameter, kCBLDomain);
}

/**
 * 5. TestCreateVectorIndex
 * Description
 *     Using the default configuration, test that the vector index can be created from
 *     the embedded vectors in the documents. The test also verifies that the created
 *     index can be used in the query.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     4. Check that the index is created without an error returned.
 *     5. Get index names from the _default.words collection and check that the index
 *       names contains “words_index”.
 *     6. Create an SQL++ query:
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     7. Check the explain() result of the query to ensure that the "words_index" is used.
 *     8. Execute the query and check that 20 results are returned.
 *     9. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     10. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCreateVectorIndex", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    auto results = executeWordsQuery(20);
    CHECK(CountResults(results) == 20);
    CHECK(isIndexTrained());
    CBLResultSet_Release(results);
}

/**
 * 6. TestUpdateVectorIndex
 * Description
 *     Test that the vector index created from the embedded vectors will be updated
 *     when documents are changed. The test also verifies that the created index can be
 *     used in the query.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query:
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           WHERE vector_match(words_index, <dinner vector>)
 *           LIMIT 350
 *     6. Check the explain() result of the query to ensure that the "words_index" is used.
 *     7. Execute the query and check that 300 results are returned.
 *     8. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     9. Update the documents:
 *         - Create _default.words.word301 with the content from _default.extwords.word1
 *         - Create _default.words.word302 with the content from _default.extwords.word2
 *         - Update _default.words.word1 with the content from _default.extwords.word3
 *         - Delete _default.words.word2
 *     10. Execute the query again and check that 301 results are returned, and
 *         - word301 and word302 are included.
 *         - word1’s word is updated with the word from _default.extwords.word3
 *         - word2 is not included.
 *     11. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestUpdateVectorIndex", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    auto results = executeWordsQuery(350);
    CHECK(CountResults(results) == 300);
    CHECK(isIndexTrained());
    CBLResultSet_Release(results);
    
    // Update docs:
    CBLError error {};
    auto doc1 = CBLCollection_GetDocument(extwordsCollection, "word1"_sl, &error);
    REQUIRE(doc1);
    copyDocument(wordsCollection, "word301", doc1);
    
    auto doc2 = CBLCollection_GetDocument(extwordsCollection, "word2"_sl, &error);
    REQUIRE(doc2);
    copyDocument(wordsCollection, "word302", doc2);
    
    auto doc3 = CBLCollection_GetDocument(extwordsCollection, "word3"_sl, &error);
    REQUIRE(doc3);
    copyDocument(wordsCollection, "word1", doc3);
    
    REQUIRE(CBLCollection_DeleteDocumentByID(wordsCollection, "word2"_sl, &error));
    
    // Query:
    results = executeWordsQuery(350);
    
    // Check results:
    auto map = mapWordResults(results);
    CHECK(map.size() == 301);
    CHECK(map["word301"] == Dict(CBLDocument_Properties(doc1))["word"].asstring());
    CHECK(map["word302"] == Dict(CBLDocument_Properties(doc2))["word"].asstring());
    CHECK(map["word1"] == Dict(CBLDocument_Properties(doc3))["word"].asstring());
    CHECK(map.count("word2") == 0);
    
    CBLDocument_Release(doc1);
    CBLDocument_Release(doc2);
    CBLDocument_Release(doc3);
    
    CBLResultSet_Release(results);
}

/**
 * 7. TestCreateVectorIndexWithInvalidVectors
 * Description
 *     Using the default configuration, test that when creating the vector index with
 *     invalid vectors, the invalid vectors will be skipped from indexing.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Update documents:
 *         - Update _default.words word1 with "vector" = null
 *         - Update _default.words word2 with "vector" = "string"
 *         - Update _default.words word3 by removing the "vector" key.
 *         - Update _default.words word4 by removing one number from the "vector" key.
 *     4. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     5. Check that the index is created without an error returned.
 *     6. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 350
 *     7. Execute the query and check that 296 results are returned, and the results
 *        do not include document word1, word2, word3, and word4.
 *     8. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     9. Update an already index vector with an invalid vector.
 *         - Update _default.words word5 with "vector" = null.
 *     10. Execute the query and check that 295 results are returned, and the results
 *        do not include document word5.
 *     11. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCreateVectorIndexWithInvalidVectors", "[VectorSearch]") {
    CBLError error {};
    auto doc = CBLCollection_GetMutableDocument(wordsCollection, "word1"_sl, &error);
    REQUIRE(doc);
    auto props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetNull(props, "vector"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word2"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "vector"_sl, "string"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word3"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    FLMutableDict_Remove(props, "vector"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word4"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    auto vector = FLMutableDict_GetMutableArray(props, "vector"_sl);
    FLMutableArray_Remove(vector, 0, 1);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    // Query:
    auto results = executeWordsQuery(350);
    CHECK(isIndexTrained());
    
    // Check results:
    auto map = mapWordResults(results);
    CHECK(map.size() == 296);
    CHECK(map.count("word1") == 0);
    CHECK(map.count("word2") == 0);
    CHECK(map.count("word3") == 0);
    CHECK(map.count("word4") == 0);
    
    CBLResultSet_Release(results);
    
    // Update the doc:
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word5"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetNull(props, "vector"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    // Query:
    results = executeWordsQuery(350);
    
    // Check results:
    map = mapWordResults(results);
    CHECK(map.size() == 295);
    CHECK(map.count("word5") == 0);
    
    CBLResultSet_Release(results);
}

/**
 * 8. TestCreateVectorIndexUsingPredictionModel
 * Description
 *     Using the default configuration, test that the vector index can be created from
 *     the vectors returned by a predictive model.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Register  "WordEmbedding" predictive model defined in section 2.
 *     4. Create a vector index named "words_pred_index" in _default.words collection.
 *         - expression: "prediction(WordEmbedding, {"word": word}).vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     5. Check that the index is created without an error returned.
 *     6. Create an SQL++ query:
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(prediction(WordEmbedding, {'word': word}).vector, $dinerVector)
 *           LIMIT 350
 *     7. Check the explain() result of the query to ensure that the "words_pred_index" is used.
 *     8. Execute the query and check that 300 results are returned.
 *     9. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     10. Update the vector index:
 *         - Create _default.words.word301 with the content from _default.extwords.word1
 *         - Create _default.words.word302 with the content from _default.extwords.word2
 *         - Update _default.words.word1 with the content from _default.extwords.word3
 *         - Delete _default.words.word2
 *     11. Execute the query and check that 301 results are returned.
 *         - word301 and word302 are included.
 *         - word1 is updated with the word from _default.extwords.word2.
 *         - word2 is not included.
 *     12. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCreateVectorIndexUsingPredictionModel", "[VectorSearch]") {
    // The test spec creates the index named "words_pred_index", but it's ok to use any index name for the test.
    auto expr = "prediction(WordEmbedding,{\"word\": word}).vector"_sl;
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, expr, 300, 8 };
    createWordsIndex(config); // index name is defined in kWordsIndexName.
    
    // Query:
    auto results = executeWordsQuery(350, expr.asString());
    CHECK(CountResults(results) == 300);
    CHECK(isIndexTrained());
    CBLResultSet_Release(results);
    
    // Update docs:
    CBLError error {};
    auto doc1 = CBLCollection_GetDocument(extwordsCollection, "word1"_sl, &error);
    REQUIRE(doc1);
    copyDocument(wordsCollection, "word301", doc1);
    
    auto doc2 = CBLCollection_GetDocument(extwordsCollection, "word2"_sl, &error);
    REQUIRE(doc2);
    copyDocument(wordsCollection, "word302", doc2);
    
    auto doc3 = CBLCollection_GetDocument(extwordsCollection, "word3"_sl, &error);
    REQUIRE(doc3);
    copyDocument(wordsCollection, "word1", doc3);
    
    REQUIRE(CBLCollection_DeleteDocumentByID(wordsCollection, "word2"_sl, &error));
    
    // Query:
    results = executeWordsQuery(350, expr.asString());
    
    // Check results:
    auto map = mapWordResults(results);
    CHECK(map.size() == 301);
    CHECK(map["word301"] == Dict(CBLDocument_Properties(doc1))["word"].asstring());
    CHECK(map["word302"] == Dict(CBLDocument_Properties(doc2))["word"].asstring());
    CHECK(map["word1"] == Dict(CBLDocument_Properties(doc3))["word"].asstring());
    CHECK(map.count("word2") == 0);
    
    CBLDocument_Release(doc1);
    CBLDocument_Release(doc2);
    CBLDocument_Release(doc3);
    
    CBLResultSet_Release(results);
}

/**
 * 9. TestCreateVectorIndexUsingPredictiveModelWithInvalidVectors
 * Description
 *     Using the default configuration, test that when creating the vector index using
 *     a predictive model with invalid vectors, the invalid vectors will be skipped
 *     from indexing.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Register  "WordEmbedding" predictive model defined in section 2.
 *     4. Update documents.
 *         - Update _default.words word1 with "vector" = null
 *         - Update _default.words word2 with "vector" = "string"
 *         - Update _default.words word3 by removing the "vector" key.
 *         - Update _default.words word4 by removing one number from the "vector" key.
 *     5. Create a vector index named "words_prediction_index" in _default.words collection.
 *         - expression: "prediction(WordEmbedding, {"word": word}).embedding"
 *         - dimensions: 300
 *         - centroids: 8
 *     6. Check that the index is created without an error returned.
 *     7. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(prediction(WordEmbedding, {'word': word}).vector, $dinerVector)
 *           LIMIT 350
 *     8. Check the explain() result of the query to ensure that the "words_predi_index" is used.
 *     9. Execute the query and check that 296 results are returned and the results
 *        do not include word1, word2, word3, and word4.
 *     10. Verify that the index was trained by checking that the “Untrained index; queries may be slow” doesn’t exist in the log.
 *     11. Update an already index vector with a non existing word in the database.
 *         - Update _default.words.word5 with “word” = “Fried Chicken”.
 *     12. Execute the query and check that 295 results are returned, and the results
 *         do not include document word5.
 *     13. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCreateVectorIndexUsingPredictionModelWithInvalidVectors", "[VectorSearch]") {
    CBLError error {};
    auto doc = CBLCollection_GetMutableDocument(wordsCollection, "word1"_sl, &error);
    REQUIRE(doc);
    auto props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetNull(props, "vector"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word2"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "vector"_sl, "string"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word3"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    FLMutableDict_Remove(props, "vector"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word4"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    auto vector = FLMutableDict_GetMutableArray(props, "vector"_sl);
    FLMutableArray_Remove(vector, 0, 1);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    // The test spec creates the index named "words_pred_index", but it's ok to use any index name for the test.
    auto expr = "prediction(WordEmbedding,{\"word\": word}).vector"_sl;
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, expr, 300, 8 };
    createWordsIndex(config); // index name is defined in kWordsIndexName.
    
    // Query:
    auto results = executeWordsQuery(350, expr.asString());
    CHECK(isIndexTrained());
    
    // Check results:
    auto map = mapWordResults(results);
    CHECK(map.size() == 296);
    CHECK(map.count("word1") == 0);
    CHECK(map.count("word2") == 0);
    CHECK(map.count("word3") == 0);
    CHECK(map.count("word4") == 0);
    
    CBLResultSet_Release(results);
    
    // Update the doc:
    doc = CBLCollection_GetMutableDocument(wordsCollection, "word5"_sl, &error);
    REQUIRE(doc);
    props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "word"_sl, "Fried Chicken"_sl);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    // Query:
    results = executeWordsQuery(350, expr.asString());
    
    // Check results:
    map = mapWordResults(results);
    CHECK(map.size() == 295);
    CHECK(map.count("word5") == 0);
    
    CBLResultSet_Release(results);
}

/**
 * 10. TestCreateVectorIndexWithSQ
 * Description
 *     Using different types of the Scalar Quantizer Encoding, test that the vector
 *     index can be created and used.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *         - encoding: ScalarQuantizer(type: SQ4)
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     6. Check the explain() result of the query to ensure that the "words_index" is used.
 *     7. Execute the query and check that 20 results are returned.
 *     8. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     9. Delete the "words_index".
 *     10. Reset the custom logger.
 *     11. Repeat Step 2 – 10 by using SQ6 and SQ8 respectively.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCreateVectorIndexWithSQ", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    
    SECTION("SQ4") {
        config.encoding = CBLVectorEncoding_CreateScalarQuantizer(kCBLSQ4);
    }
    
    SECTION("SQ6") {
        config.encoding = CBLVectorEncoding_CreateScalarQuantizer(kCBLSQ6);
    }
    
    SECTION("SQ8") {
        config.encoding = CBLVectorEncoding_CreateScalarQuantizer(kCBLSQ8);
    }
    
    createWordsIndex(config);
    
    auto results = executeWordsQuery(20);
    CHECK(CountResults(results) == 20);
    CHECK(isIndexTrained());
    
    deleteWordsIndex();
    
    resetLog();
    CBLVectorEncoding_Free(config.encoding);
    CBLResultSet_Release(results);
}

/**
 * 11. TestCreateVectorIndexWithNoneEncoding
 * Description
 *     Using the None Encoding, test that the vector index can be created and used.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *         - encoding: None
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     6. Check the explain() result of the query to ensure that the "words_index" is used.
 *     7. Execute the query and check that 20 results are returned.
 *     8. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     9. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "testCreateVectorIndexWithNoneEncoding", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    config.encoding = CBLVectorEncoding_CreateNone();
    createWordsIndex(config);
    
    auto results = executeWordsQuery(20);
    CHECK(CountResults(results) == 20);
    CHECK(isIndexTrained());
    
    CBLVectorEncoding_Free(config.encoding);
    CBLResultSet_Release(results);
}

/**
 * 12. TestCreateVectorIndexWithPQ
 * Description
 *     Using the PQ Encoding, test that the vector index can be created and used. The
 *     test also tests the lower and upper bounds of the PQ’s bits.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *         - encoding : PQ(subquantizers: 5 bits: 8)
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     6. Check the explain() result of the query to ensure that the "words_index" is used.
 *     7. Execute the query and check that 20 results are returned.
 *     8. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     9. Delete the “words_index”.
 *     10. Reset the custom logger.
 *     11. Repeat steps 2 to 10 by changing the PQ’s bits to 4 and 12 respectively.
 */
TEST_CASE_METHOD(VectorSearchTest, "testCreateVectorIndexWithPQ", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    
    SECTION("4-bits") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(5, 4);
    }
    
    SECTION("8-bits") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(5, 8);
    }
    
    SECTION("12-bits") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(5, 12);
    }
     
    createWordsIndex(config);
    
    auto results = executeWordsQuery(20);
    CHECK(CountResults(results) == 20);
    
    deleteWordsIndex();
    
    resetLog();
    CBLVectorEncoding_Free(config.encoding);
    CBLResultSet_Release(results);
}

/**
 * 13. TestSubquantizersValidation
 * Description
 *     Test that the PQ’s subquantizers value is validated with dimensions correctly.
 *     The invalid argument exception should be thrown when the vector index is created
 *     with invalid subquantizers which are not a divisor of the dimensions or zero.
 * Steps
 *     1. Copy database words_db.
 *     2. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *         - PQ(subquantizers: 2, bits: 8)
 *     3. Check that the index is created without an error returned.
 *     4. Delete the "words_index".
 *     5. Repeat steps 2 to 4 by changing the subquantizers to
 *       3, 4, 5, 6, 10, 12, 15, 20, 25, 30, 50, 60, 75, 100, 150, and 300.
 *     6. Repeat step 2 to 4 by changing the subquantizers to 0 and 7.
 *     7. Check that an invalid argument exception is thrown.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestSubquantizersValidation : Valid", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    
    SECTION("Subquantizer - 2") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(2, 8);
    }
    
    SECTION("Subquantizer - 3") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(3, 8);
    }
    
    SECTION("Subquantizer - 150") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(150, 8);
    }
    
    SECTION("Subquantizer - 300") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(300, 8);
    }
    
    createWordsIndex(config);
    
    deleteWordsIndex();
}

TEST_CASE_METHOD(VectorSearchTest, "TestSubquantizersValidation : Invalid", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    
    SECTION("Subquantizer - 0") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(0, 8);
    }
    
    SECTION("Subquantizer - 7") {
        config.encoding = CBLVectorEncoding_CreateProductQuantizer(7, 8);
    }
    
    ExpectingExceptions x;
    CBLError error {};
    CHECK(!CBLCollection_CreateVectorIndex(wordsCollection, kWordsIndexName, config, &error));
    CheckError(error, kCBLErrorInvalidParameter, kCBLDomain);
}

/**
 * 14. TestCreateVectorIndexWithFixedTrainingSize
 * Description
 *     Test that the vector index can be created and trained when minTrainingSize
 *     equals to maxTrainingSize.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *         - minTrainingSize: 100 and maxTrainingSize: 100
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     5. Check the explain() result of the query to ensure that the "words_index" is used.
 *     6. Execute the query and check that 20 results are returned.
 *     7. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     8. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCreateVectorIndexWithFixedTrainingSize", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    config.minTrainingSize = 100;
    config.maxTrainingSize = 100;
    createWordsIndex(config);
    
    auto results = executeWordsQuery(20);
    CHECK(CountResults(results) == 20);
    CHECK(isIndexTrained());
    
    CBLResultSet_Release(results);
}

/**
 * 15. TestValidateMinMaxTrainingSize
 * Description
 *     Test that the minTrainingSize and maxTrainingSize values are validated
 *     correctly. The invalid argument exception should be thrown when the vector index
 *     is created with invalid minTrainingSize or maxTrainingSize.
 * Steps
 *     1. Copy database words_db.
 *     2. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *         - minTrainingSize: 1 and maxTrainingSize: 100
 *     3. Check that the index is created without an error returned.
 *     4. Delete the "words_index"
 *     5. Repeat Step 2 with the following cases:
 *         - minTrainingSize = 0 and maxTrainingSize 0
 *         - minTrainingSize = 0 and maxTrainingSize 100
 *         - minTrainingSize = 10 and maxTrainingSize 9
 *     6. Check that an invalid argument exception was thrown for all cases in step 4.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestValidateMinMaxTrainingSize", "[VectorSearch]") {
    // Valid minTrainingSize / maxTrainingSize:
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    config.minTrainingSize = 1;
    config.maxTrainingSize = 100;
    CBLError error {};
    CHECK(CBLCollection_CreateVectorIndex(wordsCollection, kWordsIndexName, config, &error));
    
    // Invalid minTrainingSize / maxTrainingSize:
    config.minTrainingSize = 10;
    config.maxTrainingSize = 9;
    
    ExpectingExceptions x;
    CHECK(!CBLCollection_CreateVectorIndex(wordsCollection, kWordsIndexName, config, &error));
    CheckError(error, kCBLErrorInvalidParameter, kCBLDomain);
}

/**
 * 16. TestQueryUntrainedVectorIndex
 * Description
 *     Test that the untrained vector index can be used in queries.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *         - minTrainingSize: 400
 *         - maxTrainingSize: 500
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     6. Check the explain() result of the query to ensure that the "words_index" is used.
 *     7. Execute the query and check that 20 results are returned.
 *     8. Verify that the index was not trained by checking that the “Untrained index;
 *       queries may be slow” message exists in the log.
 *     9. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestQueryUntrainedVectorIndex", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    config.minTrainingSize = 400;
    config.maxTrainingSize = 500;
    createWordsIndex(config);
    
    auto results = executeWordsQuery(20);
    CHECK(CountResults(results) == 20);
    CHECK(!isIndexTrained());
    
    CBLResultSet_Release(results);
}

/**
 * 17. TestCreateVectorIndexWithDistanceMetric
 * Description
 *     Test that the vector index can be created with all supported distance metrics.
 * Steps
 *     1. Copy database words_db.
 *     2. For each distance metric types : euclideanSquared, euclidean, cosine, and dot,
 *       create a vector index named "words_index" in _default.words collection:
 *        - expression: "vector"
 *        - dimensions: 300
 *        - centroids : 8
 *        - metric: <distance-metric>
 *     3. Check that the index is created without an error returned.
 *     4. Create an SQL++ query with the correspoding SQL++ metric name string:
 *       "EUCLIDEAN_SQUARED", "EUCLIDEAN", "COSINE", and "DOT"
 *        - SELECT meta().id, word
 *          FROM _default.words
 *          ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector, "<metric-name>")
 *          LIMIT 20
 *     5. Check the explain() result of the query to ensure that the "words_index" is used.
 *     6. Verify that the index was trained.
 *     7. Execute the query and check that 20 results are returned.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestVectorIndexDistanceMetric", "[VectorSearch]") {
    CBLDistanceMetric metrics[] = { kCBLDistanceMetricEuclideanSquared,
                                    kCBLDistanceMetricEuclidean,
                                    kCBLDistanceMetricCosine,
                                    kCBLDistanceMetricDot };
    string metricNames[] = { "EUCLIDEAN_SQUARED", "EUCLIDEAN", "COSINE", "DOT" };
    
    for (int i = 0; i < sizeof(metrics)/sizeof(metrics[0]); i++) {
        CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
        config.metric = metrics[i];
        createWordsIndex(config);
        auto results = executeWordsQuery(20, "vector", metricNames[i], "");
        CHECK(CountResults(results) == 20);
        CBLResultSet_Release(results);
    }
}

/**
 * 19. TestCreateVectorIndexWithExistingName
 * Description
 *     Test that creating a new vector index with an existing name is fine if the index
 *     configuration is the same or not.
 * Steps
 *     1. Copy database words_db.
 *     2. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     3. Check that the index is created without an error returned.
 *     4. Repeat step 2 and check that the index is created without an error returned.
 *     5. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vectors"
 *         - dimensions: 300
 *         - centroids: 8
 *     6. Check that the index is created without an error returned.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestCreateVectorIndexWithExistingName", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    createWordsIndex(config);
    
    config.expression = "vectors"_sl;
    config.dimensions = 300;
    config.centroids = 8;
    
    createWordsIndex(config);
}

/**
 * 20. TestDeleteVectorIndex
 * Description
 *     Test that creating a new vector index with an existing name is fine if the index
 *     configuration is the same. Otherwise, an error will be returned.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vectors"
 *         - dimensions: 300
 *         - centroids: 8
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     6. Check the explain() result of the query to ensure that the "words_index" is used.
 *     7. Execute the query and check that 20 results are returned.
 *     8. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     9. Delete index named "words_index".
 *     10. Check that getIndexes() does not contain "words_index".
 *     11. Create the same query again and check that a CouchbaseLiteException is returned
 *        as the index doesn’t exist.
 *     12. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestDeleteVectorIndex", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    auto results = executeWordsQuery(20);
    CHECK(CountResults(results) == 20);
    CHECK(isIndexTrained());
    CBLResultSet_Release(results);
    
    deleteWordsIndex();
    
    ExpectingExceptions x;
    results = executeWordsQuery(20, "vector", "", "", kCBLErrorMissingIndex);
    CHECK(!results);
}

/**
 * 21. TestVectorMatchOnNonExistingIndex
 * Description
 *     Test that an error will be returned when creating a vector match query that uses
 *     a non existing index.
 * Steps
 *     1. Copy database words_db.
 *     2. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     3. Check that a CouchbaseLiteException is returned as the index doesn’t exist.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestVectorMatchOnNonExistingIndex", "[VectorSearch]") {
    ExpectingExceptions x;
    CBLError error {};
    auto query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(wordQueryString(20)), nullptr, &error);
    CHECK(!query);
    CheckError(error, kCBLErrorMissingIndex, kCBLDomain);
}

/**
 * 23. TestVectorMatchLimitBoundary
 * Description
 *     Test vector_match’s limit boundary which is between 1 - 10000 inclusively. When
 *     creating vector_match queries with an out-out-bound limit, an error should be
 *     returned.
 * Steps
 *     1. Copy database words_db.
 *     2. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     3. Check that the index is created without an error returned.
 *     4. Create an SQL++ query.
 *         - SELECT meta().id, word
 *           FROM _default.words
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT <limit>
 *         - limit : 1 and 10000
 *     5. Check that the query can be created without an error.
 *     6. Repeat step 4 with the limit: -1, 0, and 10001
 *     7. Check that a CouchbaseLiteException is returned when creating the query.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestVectorMatchLimitBoundary", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    CBLQuery* query = nullptr;
    
    ExpectingExceptions x;
    CBLError error {};
    SECTION("Valid Limit : 1") {
        query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(wordQueryString(1)), nullptr, &error);
        CHECK(query);
    }
    
    SECTION("Valid Limit : 10000") {
        query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(wordQueryString(10000)), nullptr, &error);
        CHECK(query);
    }
    
    SECTION("Invalid Limit : -1") {
        query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(wordQueryString(-1)), nullptr, &error);
        CHECK(!query);
        CheckError(error, kCBLErrorInvalidQuery, kCBLDomain);
    }
    
    SECTION("Invalid Limit : 0") {
        query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(wordQueryString(0)), nullptr, &error);
        CHECK(!query);
        CheckError(error, kCBLErrorInvalidQuery, kCBLDomain);
    }
    
    SECTION("Invalid Limit : 10001") {
        query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(wordQueryString(10001)), nullptr, &error);
        CHECK(!query);
        CheckError(error, kCBLErrorInvalidQuery, kCBLDomain);
    }
    
    CBLQuery_Release(query);
}

/**
 * 24. TestHybridVectorSearch
 * Description
 *     Test a simple hybrid search with WHERE clause.
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query.
 *         - SELECT word, catid
 *           FROM _default.words
 *           WHERE catid = "cat1"
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 300
 *     6. Check that the query can be created without an error.
 *     7. Check the explain() result of the query to ensure that the "words_index" is used.
 *     8. Execute the query and check that the number of results returned is 50
 *       (there are 50 words in catid=1), and the results contain only catid == 'cat1'.
 *     9. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     10. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestHybridVectorSearch", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    auto results = executeWordsQuery(300, "vector", "", "catid = 'cat1'");
    CHECK(CountResults(results) == 50);
    
    CBLResultSet_Release(results);
}

/**
 * 25. TestHybridVectorSearchWithAND
 * Description
 *     Test hybrid search with multiple AND
 * Steps
 *     1. Copy database words_db.
 *     2. Register a custom logger to capture the INFO log.
 *     3. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     4. Check that the index is created without an error returned.
 *     5. Create an SQL++ query.
 *         - SELECT word, catid
 *           FROM _default.words
 *           WHERE catid = "cat1" AND word is valued
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 300
 *     6. Check that the query can be created without an error.
 *     7. Check the explain() result of the query to ensure that the "words_index" is used.
 *     8. Execute the query and check that the number of results returned is 50
 *       (there are 50 words in catid=1), and the results contain only catid == 'cat1'.
 *     9. Verify that the index was trained by checking that the “Untrained index; queries may be slow”
 *       doesn’t exist in the log.
 *     10. Reset the custom logger.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestHybridVectorSearchWithAND", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    auto results = executeWordsQuery(300, "vector", "", "word is valued AND catid = 'cat1'");
    CHECK(CountResults(results) == 50);
    
    CBLResultSet_Release(results);
}

/**
 * 26. TestInvalidHybridVectorSearchWithOR
 * Description
 *     Test that APPROX_VECTOR_DISTANCE cannot be used with OR expression.
 * Steps
 *     1. Copy database words_db.
 *     2. Create a vector index named "words_index" in _default.words collection.
 *         - expression: "vector"
 *         - dimensions: 300
 *         - centroids: 8
 *     3. Check that the index is created without an error returned.
 *     4. Create an SQL++ query.
 *         - SELECT word, catid
 *           FROM _default.words
 *           WHERE APPROX_VECTOR_DISTANCE(vector, $dinerVector) < 10 OR catid = 'cat1'
 *           ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *           LIMIT 20
 *     5. Check that a CouchbaseLiteException is returned when creating the query.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestInvalidHybridVectorSearchWithOR", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);
    
    ExpectingExceptions x;
    CBLError error {};
    auto sql = wordQueryString(20, "vector", "", "APPROX_VECTOR_DISTANCE(vector, $vector) < 10 OR catid = 'cat1'");
    auto query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(sql), nullptr, &error);
    CHECK(!query);
    CheckError(error, kCBLErrorInvalidQuery, kCBLDomain);
}

/**
 * 27. TestIndexVectorInBase64
 *
 * Description
 * Test that the vector in Base64 string can be indexed.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Get the vector value from _default.words.word49's vector property as an array of floats.
 * 3. Convert the array of floats from Step 2 into binary data and then into Base64 string.
 *     - See "Vector in Base64 for Lunch" section for the pre-calculated base64 string
 * 4. Update _default.words.word49 with "vector" = Base64 string from Step 3.
 * 5. Create a vector index named "words_index" in _default.words collection.
 *     - expression: "vector"
 *     - dimensions: 300
 *     - centroids : 8
 * 6. Check that the index is created without an error returned.
 * 7. Create an SQL++ query:
 *     - SELECT meta().id, word
 *       FROM _default.words
 *       ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *       LIMIT 20
 * 8. Execute the query and check that 20 results are returned.
 * 9. Check that the result also contains doc id = word49.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestIndexVectorInBase64", "[VectorSearch]") {
    CBLError error {};
    auto doc = CBLCollection_GetMutableDocument(wordsCollection, "word49"_sl, &error);
    REQUIRE(doc);
    auto props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetString(props, "vector"_sl, kLunchVectorBase64);
    REQUIRE(CBLCollection_SaveDocument(wordsCollection, doc, &error));
    CBLDocument_Release(doc);
    
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    createWordsIndex(config);

    auto results = executeWordsQuery(20);
    auto docIDs = docIDResults(results);
    CHECK(docIDs.size() == 20);
    CHECK(find(docIDs.begin(), docIDs.end(), "word49") != docIDs.end());

    CBLVectorEncoding_Free(config.encoding);
    CBLResultSet_Release(results);
}

/**
 * 28. TestNumProbes
 *
 * Description
 * Test that the numProces specified is effective.
 *
 * Steps
 * 1. Copy database words_db.
 * 2. Create a vector index named "words_index" in _default.words collection.
 *     - expression: "vector"
 *     - dimensions: 300
 *     - centroids : 8
 *     - numProbes: 5
 * 3. Check that the index is created without an error returned.
 * 4. Create an SQL++ query:
 *     - SELECT meta().id, word
 *       FROM _default.words
 *       ORDER BY APPROX_VECTOR_DISTANCE(vector, $dinerVector)
 *       LIMIT 300
 * 5. Execute the query and record the number of results returned.
 * 6. Repeat step 2 - 6 but change the numProbes to 1.
 * 7. Verify the number of results returned in Step 5 is larger than Step 6.
 */
TEST_CASE_METHOD(VectorSearchTest, "TestNumProbes", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    
    config.numProbes = 5;
    createWordsIndex(config);
    auto results = executeWordsQuery(300);
    auto numResultsFor5Probes = CountResults(results);
    CHECK(numResultsFor5Probes > 0);
    CBLResultSet_Release(results);
    
    config.numProbes = 1;
    createWordsIndex(config);
    results = executeWordsQuery(300);
    auto numResultsFor1Probes = CountResults(results);
    CHECK(numResultsFor1Probes > 0);
    CBLResultSet_Release(results);
    
    CHECK(numResultsFor5Probes > numResultsFor1Probes);
}

TEST_CASE_METHOD(VectorSearchTest, "TestVectorSearchWithWhereClause", "[VectorSearch]") {
    CBLVectorIndexConfiguration config { kCBLN1QLLanguage, "vector"_sl, 300, 8 };
    config.metric = kCBLDistanceMetricCosine;
    createWordsIndex(config);
    
    ExpectingExceptions x;
    CBLError error {};
    auto sql = "SELECT meta().id, word, APPROX_VECTOR_DISTANCE(vector, $vector) FROM words WHERE APPROX_VECTOR_DISTANCE(vector, $vector) < 0.5 LIMIT 100";
    auto query = CBLDatabase_CreateQuery(wordDB, kCBLN1QLLanguage, slice(sql), nullptr, &error);
    CHECK(query);
    setDinnerParameter(query);
    
    alloc_slice exp = CBLQuery_Explain(query);
    
    auto ex = exp.asString();
    
    
    auto rs = CBLQuery_Execute(query, &error);
    CHECK(rs);
    CheckNoError(error);
    CBLQuery_Release(query);
    
    vector<double> distances {};
    while (CBLResultSet_Next(rs)) {
        double distance = FLValue_AsDouble(CBLResultSet_ValueAtIndex(rs, 2));
        distances.push_back(distance);
    }
    CBLResultSet_Release(rs);
}

#endif

#endif
