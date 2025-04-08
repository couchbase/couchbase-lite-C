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
#include <inttypes.h>
#include <mutex>
#include <sstream>
using namespace std::chrono;

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLKeyPair final : public CBLRefCounted {
public:
    CBLKeyPair(C4KeyPair* key)
    : _c4KeyPair(key)
    {}

    static CBLKeyPair* CreateKeyPairWithPrivateKeyData(slice privateKeyData, slice passwordOrNull) {
        return new CBLKeyPair{C4KeyPair::fromPrivateKeyData(privateKeyData, passwordOrNull).detach()};
    }
    
    static CBLKeyPair* CreateKeyPairWithCallbacks(void* externalKey,
                                                  size_t keySizeInBits,
                                                  CBLKeyPairCallbacks callbacks) {
        typedef bool (*SignFuncPtr)(void* externalKey, C4SignatureDigestAlgorithm digestAlgorithm, C4Slice inputData, void* outSignature);

        C4ExternalKeyCallbacks c4Callbacks;
        c4Callbacks.publicKeyData = callbacks.publicKeyData;
        c4Callbacks.decrypt       = callbacks.decrypt;
        c4Callbacks.sign          = (SignFuncPtr)callbacks.sign;
        c4Callbacks.free          = callbacks.free;
        return new CBLKeyPair{C4KeyPair::fromExternal(kC4RSA, keySizeInBits, externalKey, c4Callbacks).get()};
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

    // Private:
    static CBLKeyPair* GenerateRSAKeyPair(slice passwordOrNull) {
        C4KeyPair* c4Key = C4KeyPair::generate(kC4RSA, 2048, false).detach();
        if ( !c4Key ) C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, "fails to generate a KeyPair.");
        return new CBLKeyPair{c4Key};
    }

    static CBLKeyPair* PublicKeyFromData(slice data) {
        C4KeyPair* c4Key = C4KeyPair::fromPublicKeyData(data).detach();
        if (!c4Key) C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, "fails to get PublicKey from data.");
        return new CBLKeyPair{c4Key};
    }

private:
    Retained<C4KeyPair> _c4KeyPair;
};

struct CBLCert final : public CBLRefCounted {
public:
    CBLCert(C4Cert* cert)
    : _c4Cert(cert)
    {}

    static CBLCert* CreateWithData(slice certData) {
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
    static constexpr unsigned    kCBLNotBeforeCertClockDriftOffsetInSeconds = 60;
    static constexpr const char* kCBLErrorMessageDuplicateCertificate = "Certificate already exists with the label";
    static constexpr const char* kCBLErrorMessageMissingCommonName    = "The Common Name attribute is required";

    CBLTLSIdentity(CBLKeyPair* _cbl_nullable keyPair, CBLCert* cert)
    : _cblKeyPair(keyPair)
    , _cblCert(cert)
    {}

    CBLTLSIdentity() {}

#ifdef TARGET_OS_IPHONE
    // Definition is in CBLTLSIdentity+Apple.mm
    static void StripPublicKey(C4Cert* c4cert);
#endif

    static Retained<C4Cert> CreateSelfSignedCert(CBLKeyUsages usages,
                                                 C4KeyPair* keypair,
                                                 Dict attributes,
                                                 CBLTimestamp expiration) {
        // copy attributes to a vector.
        std::vector<C4CertNameComponent> names;
        for (Dict::iterator iter(attributes); iter; ++iter) {
            slice attrID = iter.keyString();
            slice value = iter.value().asString();
            names.push_back({attrID, value});
        }

        // Create CSR:
        Retained<C4Cert> csr = C4Cert::createRequest(names, (C4CertUsage)usages, keypair);
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

        // Self sign:
        return csr->signRequest(issuerParams, keypair, nullptr);
    }

    static CBLTLSIdentity* CreateIdentity(CBLKeyUsages usages,
                                          CBLKeyPair* keypair,
                                          Dict attrs,
                                          CBLTimestamp exp) {
        if (!attrs.get(kCBLCertAttrKeyCommonName)) {
            C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, kCBLErrorMessageMissingCommonName);
        }
        
        Retained<C4Cert> cert = CreateSelfSignedCert(usages,keypair->c4KeyPair(), attrs, exp);
        return new CBLTLSIdentity{keypair, new CBLCert(cert.get())};
    }

    static CBLTLSIdentity* IdentityWithKeyPairAndCerts(CBLKeyPair* _cbl_nullable keypair,
                                                       CBLCert* cert) {
        return new CBLTLSIdentity{keypair, cert};
    }

#ifdef __OBJC__
    static const int             kErrSecDuplicateItem; // Definition is in CBLTLSIdentity+Apple.mm
#endif
    
    static bool checkCertExistAtLabel(slice label, CBLError* _cbl_nullable error) {
        // FIXME: https://issues.couchbase.com/browse/CBL-932
        C4Cert* cert = c4cert_load(label, cbl_internal::internal(error));
        bool exists = cert != nullptr;
        c4cert_release(cert);
        return exists;
    }

    static CBLTLSIdentity* _cbl_nullable CreateIdentity(CBLKeyUsages usages,
                                                        Dict attributes,
                                                        CBLTimestamp expiration,
                                                        slice label) {
        std::scoped_lock<std::mutex> lock(_mutex);
        
        bool persistent = static_cast<bool>(label);
        
#if defined(__linux__) || defined(__ANDROID__)
        if (persistent) {
            C4Error::raise(LiteCoreDomain, kC4ErrorUnsupported,
                           "Using persistent label is not supported on linux and android platform.");
        }
#endif
        
        if (!attributes.get(kCBLCertAttrKeyCommonName)) {
            C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, kCBLErrorMessageMissingCommonName);
        }
        
        if (persistent && checkCertExistAtLabel(label, nullptr)) {
            std::stringstream ss;
            ss << kCBLErrorMessageDuplicateCertificate << " " << label.asString();
#ifdef __OBJC__
            ss << "; OSStatus = " << kErrSecDuplicateItem;
#endif
            std::string errmsg = ss.str();
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
            C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, errmsg.c_str());
#pragma clang diagnostic pop
        }

        // Generate key pair:
        Retained<C4KeyPair> keyPair = C4KeyPair::generate(kC4RSA, 2048, persistent);
        
        // Create self-signed cert:
        Retained<C4Cert> c4cert = CreateSelfSignedCert(usages, keyPair.get(), attributes, expiration);
        assert(c4cert);
        
        if (persistent) {
#ifdef TARGET_OS_IPHONE
            // Workaround: Strip public key from the cert to avoid the keychain api
            // to get confused when finding the private key from the cert.
            StripPublicKey(c4cert);
#endif
            c4cert->save(false, label);
            CBL_Log(kCBLLogDomainListener, kCBLLogVerbose, "Created a self-signed identity with label=%.*s, usages=%d, expiry=%" PRId64 ", attr=%s",
                    (int)label.size, (char*)label.buf, usages, expiration,
                    attributes.toJSONString().c_str());
            // The keyPair was stored in the keystore and will be retrieved via the certificate.
            return new CBLTLSIdentity(nullptr, new CBLCert(c4cert.get()));
        } else {
            CBL_Log(kCBLLogDomainListener, kCBLLogVerbose, "Created a self-signed identity with usages=%d, expiry=%" PRId64 ", attr=%s",
                    usages, expiration, attributes.toJSONString().c_str());
            return new CBLTLSIdentity(new CBLKeyPair(keyPair.get()), new CBLCert(c4cert.get()));
        }
    }

#if !defined(__linux__) && !defined(__ANDROID__)
    
    static bool DeleteIdentityWithLabel(slice persistentLabel) {
        std::scoped_lock<std::mutex> lock(_mutex);

        // Load cert for getting the public key:
        Retained<C4Cert> c4cert;
        try {
            c4cert = C4Cert::load(persistentLabel);
        } catch (C4Error& err) {
            if (err.code != 0 && err.code != kC4ErrorNotFound)
                throw(err);
            // Otherwise, no cert to delete.
            return true;
        }
        if (!c4cert) return true;

        // Get public key from the cert:
        Retained<C4KeyPair> publicKey = c4cert->getPublicKey();
        // No longer need c4cert

        // Get private key from the public key:
        Retained<C4KeyPair> persistentKey;
        try {
            persistentKey = C4KeyPair::persistentWithPublicKey(publicKey);
        } catch (C4Error &err) {
            if (err.domain != LiteCoreDomain || err.code !=  kC4ErrorNotFound) {
                throw(err);
            }
            // Otherwise, this is an error of NotFound. It's okay and we just don't
            // need to remove it.
        }

        // Remove the cert:
        C4Cert::deleteNamed(persistentLabel);

        // Remove the keypair:
        if (persistentKey) persistentKey->removePersistent();

        CBL_Log(kCBLLogDomainReplicator, kCBLLogVerbose, "Deleted Identity %.*s", (int)persistentLabel.size, (char*)persistentLabel.buf);
        return true;
    }

    static CBLTLSIdentity* _cbl_nullable IdentityWithLabel(slice persistentLabel) {
        std::scoped_lock<std::mutex> lock(_mutex);

        Retained<C4Cert> cert = C4Cert::load(persistentLabel);
        if (!cert) return nullptr;
        else       return new CBLTLSIdentity(nullptr, new CBLCert(cert.get()));
    }
    
    static CBLTLSIdentity* _cbl_nullable IdentityWithCerts(CBLCert* cert) {
        std::scoped_lock<std::mutex> lock(_mutex);
        
        if (!cert->c4Cert()->loadPersistentPrivateKey()) {
            C4Error::raise(LiteCoreDomain, kC4ErrorCrypto, "No matching private key in keystore");
        }
        
        return new CBLTLSIdentity(nullptr, cert);
    }
    
#endif // #if !defined(__linux__) && !defined(__ANDROID__)

    CBLCert* certificates() const { return _cblCert; }

    CBLTimestamp expiration() const {
        CBLTimestamp expires = 0;
        _cblCert->getValidTimespan(nullptr, &expires);
        return expires;
    }

    CBLKeyPair* _cbl_nullable privateKey() const { return _cblKeyPair; }

private:
    Retained<CBLKeyPair> _cblKeyPair; // may be null
    Retained<CBLCert>    _cblCert;
    static inline std::mutex _mutex;
};

CBL_ASSUME_NONNULL_END

#endif // #ifdef COUCHBASE_ENTERPRISE
