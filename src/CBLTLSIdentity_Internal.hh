//
//  CBLTLSIdentity_Internal.hh
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

#include "CBLTLSIdentity.h"
#include "CBLPrivate.h"

#include "Internal.hh"
#include "c4Certificate.h"
#include "c4Certificate.hh"
#include <chrono>
#include <sstream>
using namespace std::chrono;

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLKeyPair final : public CBLRefCounted {
public:
    CBLKeyPair(C4KeyPair* key)
    : _c4KeyPair(key)
    {}

    static CBLKeyPair* RSAKeyPairWithPrivateKeyData(slice privateKeyData, slice passwordOrNull) {
        return new CBLKeyPair{C4KeyPair::fromPrivateKeyData(privateKeyData, passwordOrNull).detach()};
    }
    
    static CBLKeyPair* RSAKeyPairWithCallbacks(void* externalKey,
                                               size_t keySizeInBits,
                                               CBLKeyPairCallbacks callbacks) {
        typedef bool (*SignFuncPtr)(void* externalKey, C4SignatureDigestAlgorithm digestAlgorithm, C4Slice inputData, void* outSignature);

        C4ExternalKeyCallbacks c4Callbacks;
        c4Callbacks.publicKeyData = callbacks.publicKeyData;
        c4Callbacks.decrypt       = callbacks.decrypt;
        c4Callbacks.sign          = (SignFuncPtr)callbacks.sign;
        c4Callbacks.free          = callbacks.free;
        return new CBLKeyPair{C4KeyPair::fromExternal(kC4RSA, keySizeInBits, externalKey, c4Callbacks).detach()};
    }
    
    alloc_slice publicKeyDigest() const {
        return _c4KeyPair->getPublicKeyDigest();
    }
    
    alloc_slice publicKeyData() const {
        return _c4KeyPair->getPublicKeyData();
    }
    
    alloc_slice privateKeyData() const {
        return _c4KeyPair->getPrivateKeyData();
    }
    
    C4KeyPair* c4KeyPair() const { return _c4KeyPair; }
    
private:
    Retained<C4KeyPair> _c4KeyPair;
};

struct CBLCert final : public CBLRefCounted {
public:
    CBLCert(C4Cert* cert)
    : _c4Cert(cert)
    {}

    static CBLCert* CertFromData(slice certData) {
        return new CBLCert{C4Cert::fromData(certData).get()};
    }

    CBLCert* _cbl_nullable certNextInChain() const {
        Retained<C4Cert> next = _c4Cert->getNextInChain();
        if (next) return new CBLCert{next.get()};
        else return nullptr;
    }

    alloc_slice data(bool pemEncoded) const {
        return _c4Cert->getData(pemEncoded);
    }

    alloc_slice subjectName() const {
        return _c4Cert->getSubjectName();
    }

    alloc_slice subjectNameComponent(slice attributeKey) const {
        return _c4Cert->getSubjectNameComponent(attributeKey);
    }

    void getValidTimespan(CBLTimestamp* _cbl_nullable outCreated,
                          CBLTimestamp* _cbl_nullable outExpires) const {
        CBLTimestamp created, expires;
        c4cert_getValidTimespan(c4Cert(), &created, &expires);
        if (outCreated) *outCreated = created;
        if (outExpires) *outExpires = expires;
    }

    CBLKeyPair* publicKey() const {
        return new CBLKeyPair{_c4Cert->getPublicKey().get()};
    }
    
    C4Cert* c4Cert() const { return _c4Cert; }

private:
    Retained<C4Cert> _c4Cert;
};

struct CBLTLSIdentity final : public CBLRefCounted {
public:
    static constexpr unsigned kCBLNotBeforeCertClockDriftOffsetInSeconds = 60;

    CBLTLSIdentity(CBLKeyPair* keyPair, CBLCert* cert)
    : _cblKeyPair(keyPair)
    , _cblCert(cert)
    {}

    CBLTLSIdentity() {}
    
    static CBLTLSIdentity* SelfSignedCertIdentity(bool server,
                                                  CBLKeyPair* keypair,
                                                  Dict attributes,
                                                  CBLTimestamp expiration) {
        if (!attributes.get(kCBLCertAttrKeyCommonName))
            C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, "Missing Common Name when creating SelfSigedCertIdentity.");

        // copy attributes to a vector.
        std::vector<C4CertNameComponent> names;
        for (Dict::iterator iter(attributes); iter; ++iter) {
            slice attrID = iter.keyString();
            slice value = iter.value().asString();
            names.push_back({attrID, value});
        }

        // Create CSR:
        C4CertUsage usage = server ? kC4CertUsage_TLSServer : kC4CertUsage_TLSClient;
        Retained<C4Cert> csr = C4Cert::createRequest(names, usage, keypair->c4KeyPair());
        if (!csr) {
            C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, "Fails to create a signing request.");
        }

        // Construct an issuer params:
        C4CertIssuerParameters issuerParams = kDefaultCertIssuerParameters;

        // Serial Number: Use timestamp:
        auto tsp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        std::stringstream ss;
        ss << tsp;
        std::string tspStr = ss.str();
        issuerParams.serialNumber = slice(tspStr.c_str());

        // Expiration:
        unsigned expirationInSeconds = 0;
        if (expiration) {
            // CBLTimespan is in milliseconds
            expirationInSeconds = (unsigned)(expiration / 1000);
            expirationInSeconds += kCBLNotBeforeCertClockDriftOffsetInSeconds;
            issuerParams.validityInSeconds = expirationInSeconds;
        }

        // Sign:
        CBLCert* cert = new CBLCert{csr->signRequest(issuerParams, keypair->c4KeyPair(), nullptr).get()};
        return new CBLTLSIdentity{keypair, cert};
    }

    static CBLTLSIdentity* IdentityWithKeyPairAndCerts(CBLKeyPair* keypair,
                                                       CBLCert* cert) {
        return new CBLTLSIdentity{keypair, cert};
    }

    CBLCert* certificates() const { return _cblCert; }

    CBLTimestamp expiration() const {
        CBLTimestamp expires = 0;
        _cblCert->getValidTimespan(nullptr, &expires);
        return expires;
    }

    CBLKeyPair* key() const { return _cblKeyPair; }

private:
    Retained<CBLKeyPair> _cblKeyPair;
    Retained<CBLCert>    _cblCert;
};

CBL_ASSUME_NONNULL_END

#endif // #ifdef COUCHBASE_ENTERPRISE
