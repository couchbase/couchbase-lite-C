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

TEST_CASE_METHOD(TLSIdentityTest, "Self-Signed Cert Identity") {
    CBLError outError;

    CBLKeyPair* keypair = CBLKeyPair_GenerateRSAKeyPair(fleece::nullslice, &outError);
    fleece::MutableDict mdict = fleece::MutableDict::newDict();
    mdict[kCBLCertAttrKeyCommonName] = "CBLAnonymousCertificate";

    static constexpr auto validity = seconds(3141592);
    auto                    expire = system_clock::now() + validity;

    // Server ID
    CBLTLSIdentity* tlsID = CBLTLSIdentity_SelfSignedCertIdentity
        (true, keypair, mdict, duration_cast<milliseconds>(validity).count(), &outError);
    CHECK(tlsID);

    CBLCert* certOfIdentity = CBLTLSIdentity_Certificates(tlsID);
    CHECK(certOfIdentity);
    CHECK( !CBLCert_CertNextInChain(certOfIdentity) );

    // CBLTimestamp is in milliseconds
    CBLTimestamp certExpire  = CBLTLSIdentity_Expiration(tlsID);
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

    CBLTLSIdentity_Release(tlsID);
    CBLKeyPair_Release(keypair);
    CBLKeyPair_Release(pkOfCert);
}

#endif // #ifdef COUCHBASE_ENTERPRISE
