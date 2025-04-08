//
// DatabaseTest.hh
//
// Copyright © 2025 Couchbase. All rights reserved.
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

#include "CBLTest.hh"

#ifdef COUCHBASE_ENTERPRISE

class TLSIdentityTest : public CBLTest {
public:
    TLSIdentityTest() {}
    ~TLSIdentityTest() {}

#ifdef __APPLE__
    class ExternalKey {
    public:
        static ExternalKey* generateRSA(unsigned keySizeInBits);

        ~ExternalKey();

        bool publicKeyData(void* output, size_t outputMaxLen, size_t* outputLen);
        bool decrypt(fleece::slice input, void *output, size_t output_max_len, size_t *output_len);
        bool sign(CBLSignatureDigestAlgorithm mbedDigestAlgorithm, fleece::slice inputData, void *outSignature);

    private:
        struct Impl;
        ExternalKey(Impl*);
        std::unique_ptr<Impl> _impl;
    };
#endif //#ifdef __APPLE__
};

#endif // #ifdef COUCHBASE_ENTERPRISE
