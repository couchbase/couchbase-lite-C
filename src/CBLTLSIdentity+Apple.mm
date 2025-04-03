//
// CBLDatabase+Apple.mm
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
#include "CBLTLSIdentity_Internal.hh"
#include "c4Error.h"
#include <sstream>

#ifdef COUCHBASE_ENTERPRISE

static bool _keyExists(SecKeyRef keyRef) {
    NSDictionary* params = @{
        (id)kSecClass:              (id)kSecClassKey,
        (id)kSecValueRef:           (__bridge id)keyRef
    };
    OSStatus status = SecItemCopyMatching((CFDictionaryRef)params, nullptr);
    return status == errSecSuccess;
}

static bool identityExists(SecIdentityRef identityRef) {
    SecKeyRef keyRef;
    OSStatus status = SecIdentityCopyPrivateKey(identityRef, &keyRef);
    CFAutorelease(keyRef);
    return status == errSecSuccess && _keyExists(keyRef);
}

inline fleece::slice data2slice(NSData* data) {
    return {data.bytes, data.length};
}

inline NSData* slice2data(fleece::slice slice) {
    return slice.uncopiedNSData();
}

static NSString* slice2string(fleece::slice s) {
    if (!s.buf)
        return nil;
    return [[NSString alloc] initWithBytes: s.buf length: s.size encoding:NSUTF8StringEncoding];
}

inline NSString* sliceResult2string(C4SliceResult slice) {
    return slice2string(fleece::alloc_slice(slice));
}

static C4Error createSecError(OSStatus status, const char* _Nullable desc) {
    assert(status != errSecSuccess);
    std::stringstream ss;
    if (desc) ss << desc << "; ";
    ss << "OSStatus = " << (int)status;
    std::string errmsg = ss.str();
    return c4error_make(LiteCoreDomain, kC4ErrorCrypto, fleece::slice(errmsg));
}

static fleece::alloc_slice _toPEM(SecCertificateRef certRef) /* may throw */ {
    NSData* data = (NSData*)CFBridgingRelease(SecCertificateCopyData(certRef));
    fleece::Retained<C4Cert> c4cert = C4Cert::fromData(data2slice(data));
    fleece::alloc_slice pem = c4cert_copyData(c4cert, true /* PEM Format */);
    return pem;
}

static NSData* __nullable toPEM(NSArray* secCerts) /* may throw */ {
    NSMutableData* data = [NSMutableData data];
    for (NSUInteger i = 0; i < secCerts.count; i++) {
        SecCertificateRef certRef = (__bridge SecCertificateRef)[secCerts objectAtIndex: i];
        fleece::alloc_slice pem = _toPEM(certRef);
        if (!pem.buf)
            return nil;
        [data appendData: slice2data(pem)];
    }
    return data;
}

CBLTLSIdentity* CBLTLSIdentity_CreateWithSecIdentity(SecIdentityRef identity,
                                                     NSArray* certs,
                                                     CBLError* outError) noexcept {
    try {
        if ( !identityExists(identity) ) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "The identity is not present in the KeyChain");
        }

        SecCertificateRef certRef;
        OSStatus status = SecIdentityCopyCertificate(identity, &certRef);
        if (status != errSecSuccess) {
            C4Error::raise(createSecError(status, "Couldn't get certificate from the identity"));
        }
        // Successfully fetched the cert by identity into certRef

        NSMutableArray* certChain = [NSMutableArray arrayWithObject: CFBridgingRelease(certRef)];
        if (certs) {
            [certChain addObjectsFromArray: certs];
        }

        NSData* certData = toPEM(certChain);
        if (!certData)
            return nullptr;

        fleece::Retained<CBLCert> cblCert = CBLCert::CreateWithData(data2slice(certData));
        if (!cblCert) {
            CBL_Log(kCBLLogDomainListener, kCBLLogWarning, "Couldn't convert certs to C4Cert");
            return nullptr;
        }

        return retain(CBLTLSIdentity::CreateWithKeyPairAndCerts(nullptr, cblCert.get()));
    } catchAndBridge(outError);
}

#ifdef TARGET_OS_IPHONE
static NSData* _getPublicKeyHash(SecKeyRef keyRef) {
    NSDictionary* publicKeyAttrs = (NSDictionary*)CFBridgingRelease(SecKeyCopyAttributes(keyRef));
    NSData* publicKeyHash = (NSData*)publicKeyAttrs[(id)kSecAttrApplicationLabel];
    assert(publicKeyHash);
    return publicKeyHash;
}

static SecKeyRef __nullable _getPublicKey(SecCertificateRef certRef, CBLError* _Nullable error) {
    SecTrustRef trustRef;
    SecPolicyRef policyRef = SecPolicyCreateBasicX509();
    CFAutorelease(policyRef);

    OSStatus status = SecTrustCreateWithCertificates(certRef, policyRef, &trustRef);
    if (status != errSecSuccess) {
        if (error) *error = cbl_internal::external(createSecError(status, "Couldn't create trust from certificate"));
        return nil;
    }

    SecKeyRef publicKeyRef = NULL;
    if (@available(iOS 14.0, *)) {
        publicKeyRef = SecTrustCopyKey(trustRef);
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        publicKeyRef = SecTrustCopyPublicKey(trustRef);
#pragma clang diagnostic pop
    }

    CFRelease(trustRef);
    if (!publicKeyRef) {
        if (error) *error = cbl_internal::external(createSecError(errSecUnsupportedFormat, "Couldn't extract public key from trust"));
        return nil;
    }
    return publicKeyRef;
}

static NSData* __nullable _getPublicKeyHash(SecCertificateRef certRef, CBLError* _Nullable error) {
    SecKeyRef publicKeyRef = _getPublicKey(certRef, error);
    if (!publicKeyRef)
        return nil;
    return _getPublicKeyHash(publicKeyRef);
}

static SecCertificateRef __nullable toSecCert(C4Cert* c4cert, CBLError* _Nullable error) {
    fleece::alloc_slice certData(c4cert_copyData(c4cert, false));
    NSData* data = slice2data(certData);
    SecCertificateRef certRef = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)data);
    if (certRef == NULL) {
        if (error) *error = cbl_internal::external(createSecError(errSecUnknownFormat, "Couldn't create certificate from data"));
        return nil;
    }
    return certRef;
}

static bool _deleteKey(NSData* publicKeyHash, bool isPrivateKey, CBLError* _Nullable error) {
    id keyClass = isPrivateKey ? (id)kSecAttrKeyClassPrivate : (id)kSecAttrKeyClassPublic;
    NSDictionary* params = @{
        (id)kSecClass:                  (id)kSecClassKey,
        (id)kSecAttrKeyClass:           keyClass,
        (id)kSecAttrApplicationLabel:   publicKeyHash
    };
    OSStatus status = SecItemDelete((CFDictionaryRef)params);
    if (status != errSecSuccess && status != errSecInvalidItemRef && status != errSecItemNotFound) {
        if (error) *error = cbl_internal::external(createSecError(status, "Couldn't delete the public key from the KeyChain"));
        return false;
    }
    return true;
}

static bool deletePublicKey(SecCertificateRef certRef, CBLError* _Nullable error) {
    NSData* publicKeyHash = _getPublicKeyHash(certRef, error);
    if (!publicKeyHash)
        return false;
    return _deleteKey(publicKeyHash, false, error);
}

const int CBLTLSIdentity::kErrSecDuplicateItem = errSecDuplicateItem;

bool CBLTLSIdentity::StripPublicKey(C4Cert* c4cert, CBLError* error) {
    // KeyChain API could pick up public key instead of private key when getting a SecIdentity.
    // A work around is to remove the public key from the KeyChain. This is found when testing
    // client cert auth on iOS. See: https://forums.developer.apple.com/thread/69642.
    SecCertificateRef certRef = toSecCert(c4cert, error);
    return !!certRef && deletePublicKey(certRef, error);
}

#endif // #ifdef TARGET_OS_IPHONE
#endif // #ifdef COUCHBASE_ENTERPRISE
