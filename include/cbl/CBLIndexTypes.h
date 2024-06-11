//
//  CBLIndexTypes.h
//
// Copyright (c) 2024 Couchbase, Inc All rights reserved.
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

#pragma once
#include "CBLBase.h"
#include "CBLQueryTypes.h"

CBL_CAPI_BEGIN

/** \defgroup index  Index 
    @{ */

/** \name Index Configuration
    @{ */

/** Value Index Configuration. */
typedef struct {
    /** The language used in the expressions. */
    CBLQueryLanguage expressionLanguage;
    
    /** The expressions describing each coloumn of the index. The expressions could be specified
        in a JSON Array or in N1QL syntax using comma delimiter. */
    FLString expressions;
} CBLValueIndexConfiguration;

/** Full-Text Index Configuration. */
typedef struct {
    /** The language used in the expressions (Required). */
    CBLQueryLanguage expressionLanguage;
    
    /** The expressions describing each coloumn of the index. The expressions could be specified
        in a JSON Array or in N1QL syntax using comma delimiter. (Required) */
    FLString expressions;
    
    /** Should diacritical marks (accents) be ignored?
        Defaults to  \ref kCBLDefaultFullTextIndexIgnoreAccents.
        Generally this should be left `false` for non-English text. */
    bool ignoreAccents;
    
    /** The dominant language. Setting this enables word stemming, i.e.
        matching different cases of the same word ("big" and "bigger", for instance) and ignoring
        common "stop-words" ("the", "a", "of", etc.)

        Can be an ISO-639 language code or a lowercase (English) language name; supported
        languages are: da/danish, nl/dutch, en/english, fi/finnish, fr/french, de/german,
        hu/hungarian, it/italian, no/norwegian, pt/portuguese, ro/romanian, ru/russian,
        es/spanish, sv/swedish, tr/turkish.
     
        If left null,  or set to an unrecognized language, no language-specific behaviors
        such as stemming and stop-word removal occur. */
    FLString language;
} CBLFullTextIndexConfiguration;

#ifdef COUCHBASE_ENTERPRISE

/** An opaque object representing vector encoding config to use in CBLVectorIndexConfiguration. */
typedef struct CBLVectorEncoding CBLVectorEncoding;

/** Creates a no-encoding config to use in CBLVectorIndexConfiguration; 4 bytes per dimension, no data loss.  */
_cbl_warn_unused
CBLVectorEncoding* CBLVectorEncoding_CreateNone(void) CBLAPI;

/** Scalar Quantizer encoding type */
typedef CBL_ENUM(uint32_t, CBLScalarQuantizerType) {
    kCBLSQ4 = 0,                            ///< 4 bits per dimension
    kCBLSQ6,                                ///< 6 bits per dimension
    kCBLSQ8                                 ///< 8 bits per dimension
};

/** Creates a Scalar Quantizer encoding config to use in CBLVectorIndexConfiguration. */
_cbl_warn_unused
CBLVectorEncoding* CBLVectorEncoding_CreateScalarQuantizer(CBLScalarQuantizerType type) CBLAPI;

/** Creates a Product Quantizer encoding config to use in CBLVectorIndexConfiguration. */
_cbl_warn_unused
CBLVectorEncoding* CBLVectorEncoding_CreateProductQuantizer(unsigned subquantizers, unsigned bits) CBLAPI;

/** Frees a CBLVectorEncoding object. The encoding object can be freed after the index is created. */
void CBLVectorEncoding_Free(CBLVectorEncoding* _cbl_nullable) CBLAPI;

/** Distance metric to use in CBLVectorIndexConfiguration. */
typedef CBL_ENUM(uint32_t, CBLDistanceMetric) {
    kCBLDistanceMetricEuclidean = 0,        ///< Euclidean distance
    kCBLDistanceMetricCosine,               ///< Cosine distance (1.0 - Cosine Similarity)
};

/** ENTERPRISE EDITION ONLY
    
    Vector Index Configuration. */
typedef struct {
    /** The language used in the expressions (Required). */
    CBLQueryLanguage expressionLanguage;
    
    /** An expression returning a vector which is an array of numbers. The expression could be specified
        in a JSON Array or in N1QL syntax depending on the expressionLanguage. (Required) */
    FLString expression;
    
    /** The number of vector dimensions. (Required) */
    unsigned dimensions;
    
    /** The number of centroids which is the number buckets to partition the vectors in the index. (Required) */
    unsigned centroids;
    
    /** The boolean flag indicating that index is lazy or not. The default value is false.
     
        If the index is lazy, it will not be automatically updated when the documents in the collection are changed,
        except when the documents are deleted or purged.
     
        When configuring the index to be lazy, the expression set to the config is the expression that returns
        a value used for computing the vector.
     
        To update the lazy index, use a CBLIndexUpdater object, which can be obtained
        from a CBLIndex object. To get a CBLIndex object, call CBLCollection_GetIndex. */
    bool lazy;
    
    /** Vector encoding type. The default value is 8-bits Scalar Quantizer. */
    CBLVectorEncoding* encoding;
    
    /** Distance Metric type. The default value is euclidean distance. */
    CBLDistanceMetric metric;
    
    /** The minium number of vectors for training the index, an initial process for preparing an index based on
        the characteristics of the vectors to be indexed. Prior training, the full table scan will be peformed
        when the vector_match() function is used in the query.
        
        The default value is 25 times number of centroids.The number must be more than zero and not
        greater than maxTrainingSize.
     
        An invalid argument error will be thrown when creating the index if an invalid value is used. */
    unsigned minTrainingSize;
    
    /** The max number of vectors used when trainning the index. The default
        value is 256 times number of centroids. The number must be more than zero
        and not less than minTrainingSize. An invalid argument will be thrown
        when creating the index if an invalid value is used. */
    unsigned maxTrainingSize;
} CBLVectorIndexConfiguration;

#endif

/** @} */

/** @} */

CBL_CAPI_END
