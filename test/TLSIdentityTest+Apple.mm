//
// TLSIdentityTest+Apple.mm
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

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#include "TLSIdentityTest.hh"
#include <sstream>

#ifdef COUCHBASE_ENTERPRISE

__unused static inline void warnCFError(CFErrorRef cfError, const char *fnName) {
    auto error = (__bridge NSError*)cfError;
    auto message = error.description;
    CBL_Log(kCBLLogDomainListener, kCBLLogError, "%s failed: %s", fnName, message.UTF8String);
}

static NSData* uncopiedNSData(fleece::slice slice) {
    if (!slice.buf)
        return nil;
    return [[NSData alloc] initWithBytesNoCopy: (void*)slice.buf length: slice.size freeWhenDone: NO];
}

struct TLSIdentityTest::ExternalKey::Impl {
    Impl(SecKeyRef publicKey, SecKeyRef privateKey, unsigned keySizeInBits)
    : _publicKeyRef(publicKey)
    , _privateKeyRef(privateKey)
    , _keyLength((keySizeInBits + 7) / 8)
    {}

    ~Impl() {
        CFRelease(_publicKeyRef);
        CFRelease(_privateKeyRef);
    }

    bool publicKeyData(void* output, size_t outputMaxLen, size_t* outputLen) {
        CFErrorRef error;
        CFDataRef data = SecKeyCopyExternalRepresentation(_publicKeyRef, &error);
        if (!data) {
            warnCFError(error, "SecKeyCopyExternalRepresentation");
            return false;
        }
        fleece::slice dataSlice{CFDataGetBytePtr(data), (size_t)CFDataGetLength(data)};
        // Converting it into the DER format LiteCore expects.
        CBLKeyPair* publicKey = CBLKeyPair_PublicKeyFromData(dataSlice, nullptr);
        if (!publicKey) {
            CBL_Log(kCBLLogDomainListener, kCBLLogError, "Error from PublickKeyFromData");
            CFRelease(data);
            return false;
        }

        fleece::alloc_slice result = CBLKeyPair_PublicKeyData(publicKey);
        *outputLen = result.size;

        if (*outputLen <= outputMaxLen) {
            result.copyTo(output);
        } else {
            CBL_Log(kCBLLogDomainListener, kCBLLogError, "Key size is too big to put in the output");
        }

        CFRelease(data);
        CBLKeyPair_Release(publicKey);

        return *outputLen <= outputMaxLen;
    }

    bool decrypt(fleece::slice input, void* output, size_t outputMaxLen, size_t* outputLen) {
        // No exceptions may be thrown from this function!
        @autoreleasepool {
            CBL_Log(kCBLLogDomainListener, kCBLLogInfo, "Decrypting using Keychain private key");
            NSData* data = uncopiedNSData(input);
            CFErrorRef error;
            NSData* cleartext = CFBridgingRelease( SecKeyCreateDecryptedData(_privateKeyRef,
                                                         kSecKeyAlgorithmRSAEncryptionPKCS1,
                                                         (CFDataRef)data, &error) );
            if (!cleartext) {
                warnCFError(error, "SecKeyCreateDecryptedData");
                return false;
            }
            *outputLen = cleartext.length;
            if (*outputLen > outputMaxLen) {
                // should never happen
                CBL_Log(kCBLLogDomainListener, kCBLLogError, "outputLen is too small in callback decrypt");
                return false;
            }
            memcpy(output, cleartext.bytes, *outputLen);
            return true;
        }
    }

    bool sign(int/*mbedtls_md_type_t*/ mbedDigestAlgorithm, fleece::slice inputData, void* outSignature) {
        // No exceptions may be thrown from this function!
        CBL_Log(kCBLLogDomainListener, kCBLLogInfo, "Signing using Keychain private key");
        @autoreleasepool {
            // Map mbedTLS digest algorithm ID to SecKey algorithm ID:
            static const std::unordered_map<int, SecKeyAlgorithm> kDigestAlgorithmMap{
                {0 /*MBEDTLS_MD_NONE*/, kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw},
                {5 /*MBEDTLS_MD_SHA1*/, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1},
                {8 /*MBEDTLS_MD_SHA224*/, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224},
                {9 /*MBEDTLS_MD_SHA256*/, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256},
                {10 /*MBEDTLS_MD_SHA384*/, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384},
                {11 /*MBEDTLS_MD_SHA512*/, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512}
            };
            
            SecKeyAlgorithm digestAlgorithm = nullptr;
            if (kDigestAlgorithmMap.contains(mbedDigestAlgorithm)) {
                digestAlgorithm = kDigestAlgorithmMap.at(mbedDigestAlgorithm);
            }

            if (!digestAlgorithm) {
                CBL_Log(kCBLLogDomainListener, kCBLLogWarning, "Keychain private key: unsupported mbedTLS digest algorithm %d", mbedDigestAlgorithm);
                return false;
            }

            // Create the signature:
            NSData* data = uncopiedNSData(inputData);
            CFErrorRef error;
            NSData* sigData = CFBridgingRelease(SecKeyCreateSignature(_privateKeyRef,
                                                                      digestAlgorithm,
                                                                      (CFDataRef)data, &error));
            if (!sigData) {
                warnCFError(error, "SecKeyCreateSignature");
                return false;
            }
            assert(sigData.length == _keyLength);
            memcpy(outSignature, sigData.bytes, _keyLength);
            return true;
        }
    }

    SecKeyRef _publicKeyRef;
    SecKeyRef _privateKeyRef;
    /** Key length, in _bytes_ not bits. */
    unsigned const _keyLength;
};

TLSIdentityTest::ExternalKey* TLSIdentityTest::ExternalKey::generateRSA(unsigned keySizeInBits) {
    @autoreleasepool {
        CBL_Log(kCBLLogDomainListener, kCBLLogInfo, "Generating %d-bit RSA key-pair in Keychain", keySizeInBits);

        NSDictionary* params = @ {
            (id)kSecAttrKeyType:        (id)kSecAttrKeyTypeRSA,
            (id)kSecAttrKeySizeInBits:  @(keySizeInBits),
            (id)kSecAttrIsPermanent:    @NO,
        };

        SecKeyRef publicKey = NULL, privateKey = NULL;
        CFErrorRef error;
        privateKey = SecKeyCreateRandomKey((CFDictionaryRef)params, &error);
        if (!privateKey) {
            CBL_Log(kCBLLogDomainListener, kCBLLogWarning, "SecKeyCreateRandomKey");
            return nullptr;
        }
        publicKey = SecKeyCopyPublicKey(privateKey);
        if (!publicKey) {
            CBL_Log(kCBLLogDomainListener, kCBLLogWarning, "SecKeyCopyPublicKey");
            CFRelease(privateKey);
            return nullptr;
        }
        return new TLSIdentityTest::ExternalKey(new Impl(publicKey, privateKey, keySizeInBits));
    }
}

TLSIdentityTest::ExternalKey::ExternalKey(Impl* impl) : _impl(impl) {}
TLSIdentityTest::ExternalKey::~ExternalKey() = default;

bool TLSIdentityTest::ExternalKey::publicKeyData(void* output, size_t outputMaxLen, size_t* outputLen) {
    return _impl->publicKeyData(output, outputMaxLen, outputLen);
}

bool TLSIdentityTest::ExternalKey::decrypt(fleece::slice input, void *output, size_t output_max_len, size_t *output_len) {
    return _impl->decrypt(input, output, output_max_len, output_len);
}

bool TLSIdentityTest::ExternalKey::sign(CBLSignatureDigestAlgorithm mbedDigestAlgorithm, fleece::slice inputData, void *outSignature) {
    return _impl->sign(mbedDigestAlgorithm, inputData, outSignature);
}

#endif // #ifdef COUCHBASE_ENTERPRISE
