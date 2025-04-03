//
// DatabaseTest.cc
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

#include "CBLTest.hh"
#include "CBLPrivate.h"
#include "fleece/Mutable.hh"

#include <chrono>
#include <cmath>

using namespace std::chrono;
using namespace fleece;

#ifdef COUCHBASE_ENTERPRISE

class TLSIdentityTest : public CBLTest {
public:

    TLSIdentityTest() {
        
    }

    ~TLSIdentityTest() {
      
    }
};

TEST_CASE_METHOD(TLSIdentityTest, "Self-Signed Cert Identity", "[TSLIdentity]") {
    CBLError outError;

    CBLKeyPair* keypair = CBLKeyPair_GenerateRSAKeyPair(fleece::nullslice, &outError);
    fleece::MutableDict attributes = fleece::MutableDict::newDict();
    attributes[kCBLCertAttrKeyCommonName] = "CBLAnonymousCertificate";

    static constexpr auto validity = seconds(3141592);
    auto                    expire = system_clock::now() + validity;

    // Server ID
    CBLTLSIdentity* identity = CBLTLSIdentity_CreateIdentityWithKeyPair(kCBLKeyUsagesServerAuth,
                                                                        keypair,
                                                                        attributes,
                                                                        duration_cast<milliseconds>(validity).count(),
                                                                        &outError);
    CHECK(identity);

    CBLCert* certOfIdentity = CBLTLSIdentity_Certificates(identity);
    CHECK(certOfIdentity);
    CHECK( !CBLCert_CertNextInChain(certOfIdentity) );

    // CBLTimestamp is in milliseconds
    CBLTimestamp certExpire  = CBLTLSIdentity_Expiration(identity);
    CBLTimestamp paramExpire = duration_cast<milliseconds>(expire.time_since_epoch()).count();

    // Can differ by 60 seconds
    CHECK(std::abs(certExpire - paramExpire)/1000 < seconds(61).count());

    // checking cert of TLSIdentity
    alloc_slice subjectName = CBLCert_SubjectNameComponent(certOfIdentity, kCBLCertAttrKeyCommonName);
    CHECK(subjectName == "CBLAnonymousCertificate");

    subjectName = CBLCert_SubjectName(certOfIdentity);
    CHECK(subjectName == "CN=CBLAnonymousCertificate");

    // Check the digest of public keys of the input KeyPair and that inside the Cert.
    alloc_slice pubDigest1 = CBLKeyPair_PublicKeyDigest(keypair);
    CBLKeyPair* pkOfCert = CBLCert_PublicKey(certOfIdentity);
    CHECK(pkOfCert);
    alloc_slice pubDigest2 = CBLKeyPair_PublicKeyDigest(pkOfCert);
    CHECK( (pubDigest1 && pubDigest1 == pubDigest2) );

    CBLTLSIdentity_Release(identity);
    CBLKeyPair_Release(keypair);
    CBLKeyPair_Release(pkOfCert);
}

#if !defined(__linux__) && !defined(__ANDROID__)

TEST_CASE_METHOD(TLSIdentityTest, "Self-Signed Cert Identity With Label", "[TSLIdentity]") {
    CBLError outError{};

    slice label{"CBL_Labal"};

    fleece::MutableDict attributes = fleece::MutableDict::newDict();
    attributes[kCBLCertAttrKeyCommonName] = "CBLAnonymousCertificate";
    
    static constexpr auto validity = seconds(3141592);

    CBLTLSIdentity* identity = CBLTLSIdentity_CreateIdentity(kCBLKeyUsagesServerAuth,
                                                             attributes,
                                                             duration_cast<milliseconds>(validity).count(),
                                                             label,
                                                             &outError);

    if (outError.code) {
        alloc_slice msg = CBLError_Message(&outError);
        WARN("Error Code=" << outError.code << ", msge=" << msg.asString());
    } else {
        CHECK(identity);
    }

    // CBLTLSIdentity_IdentityWithLabel

    outError.code = 0;
    CBLTLSIdentity* identity2 = CBLTLSIdentity_IdentityWithLabel(label, &outError);
    CHECK(identity2);
    CHECK(outError.code == 0);

    // CBLTLSIdentity_DeleteIdentityWithLabel

    CHECK(CBLTLSIdentity_DeleteIdentityWithLabel(label, &outError));

    CBLTLSIdentity_Release(identity);
    CBLTLSIdentity_Release(identity2);
}

#endif // #if !defined(__linux__) && !defined(__ANDROID__)

#endif // #ifdef COUCHBASE_ENTERPRISE
