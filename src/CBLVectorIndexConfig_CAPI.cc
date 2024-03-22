//
// CBLVectorIndexConfig_CAPI.cc
//
// Copyright (C) 2024 Couchbase, Inc All rights reserved.
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


#include "CBLQuery.h"
#include "CBLVectorIndexConfig.hh"
#include "Internal.hh"

#ifdef COUCHBASE_ENTERPRISE

CBLVectorEncoding* CBLVectorEncoding_CreateNone() noexcept {
    try {
        return new CBLVectorEncodingNone();
    } catchAndWarn()
}

CBLVectorEncoding* CBLVectorEncoding_CreateScalarQuantizer(CBLScalarQuantizerType type) noexcept {
    try {
        return new CBLVectorEncodingSQ(type);
    } catchAndWarn()
}

CBLVectorEncoding* CBLVectorEncoding_CreateProductQuantizer(unsigned subquantizer, unsigned bits) noexcept {
    try {
        return new CBLVectorEncodingPQ(subquantizer, bits);
    } catchAndWarn()
}

void CBLVectorEncoding_Free(CBLVectorEncoding *enc) noexcept {
    delete enc;
}

#endif
