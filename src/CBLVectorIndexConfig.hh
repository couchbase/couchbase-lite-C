//
// CBLVectorIndexConfig.hh
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
#include "c4IndexTypes.h"

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLVectorEncoding {
    virtual ~CBLVectorEncoding()                        =default;
    virtual const C4VectorEncoding& c4encoding() const  =0;
};

namespace cbl_internal {
    struct CBLVectorEncodingNone : public CBLVectorEncoding {
        CBLVectorEncodingNone() { }
        
        virtual const C4VectorEncoding& c4encoding() const override {
            return _encoding;
        }
        
    private:
        C4VectorEncoding _encoding { kC4VectorEncodingNone };
    };

    struct CBLVectorEncodingSQ : public CBLVectorEncoding {
        CBLVectorEncodingSQ(CBLScalarQuantizerType type)
        :_type(type) {
            switch (type) {
                case kCBLSQ4: _encoding.bits = 4; break;
                case kCBLSQ6: _encoding.bits = 6; break;
                case kCBLSQ8: _encoding.bits = 8; break;
                default: C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "Invalid Scalar Quantizer Type");
            }
        }
        
        virtual const C4VectorEncoding& c4encoding() const override {
            return _encoding;
        }
        
    private:
        C4VectorEncoding _encoding { kC4VectorEncodingSQ };
        CBLScalarQuantizerType _type;
    };

    struct CBLVectorEncodingPQ : public CBLVectorEncoding {
        CBLVectorEncodingPQ(unsigned subquantizer, unsigned bits) {
            _encoding.pq_subquantizers = subquantizer;
            _encoding.bits = bits;
        }
        
        virtual const C4VectorEncoding& c4encoding() const override {
            return _encoding;
        }
        
    private:
        C4VectorEncoding _encoding { kC4VectorEncodingSQ };
    };
}

CBL_ASSUME_NONNULL_END

#endif
