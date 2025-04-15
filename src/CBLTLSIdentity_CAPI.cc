//
//  CBLTLSIdentity_CAPI.cc
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

#ifdef COUCHBASE_ENTERPRISE

#include "CBLTLSIdentity_Internal.hh"
#include "CBLPrivate.h"

#pragma mark - CONSTANTS

const FLString kCBLCertAttrKeyCommonName       = FLSTR("CN");
const FLString kCBLCertAttrKeyPseudonym        = FLSTR("pseudonym");
const FLString kCBLCertAttrKeyGivenName        = FLSTR("GN");
const FLString kCBLCertAttrKeySurname          = FLSTR("SN");
const FLString kCBLCertAttrKeyOrganization     = FLSTR("O");
const FLString kCBLCertAttrKeyOrganizationUnit = FLSTR("OU");
const FLString kCBLCertAttrKeyPostalAddress    = FLSTR("postalAddress");
const FLString kCBLCertAttrKeyLocality         = FLSTR("locality");
const FLString kCBLCertAttrKeyPostalCode       = FLSTR("postalCode");
const FLString kCBLCertAttrKeyStateOrProvince  = FLSTR("ST");
const FLString kCBLCertAttrKeyCountry          = FLSTR("C");

const FLString kCBLCertAttrKeyEmailAddress = FLSTR("rfc822Name");
const FLString kCBLCertAttrKeyHostname     = FLSTR("dNSName");
const FLString kCBLCertAttrKeyURL          = FLSTR("uniformResourceIdentifier");
const FLString kCBLCertAttrKeyIPAddress    = FLSTR("iPAddress");
const FLString kCBLCertAttrKeyRegisteredID = FLSTR("registeredID");

// KeyPair

CBLKeyPair* CBLKeyPair_CreateWithCallbacks(void* context,
                                           size_t keySizeInBits,
                                           CBLKeyPairCallbacks callbacks,
                                           CBLError* outError) noexcept {
    try {
        return retain(CBLKeyPair::CreateKeyPairWithCallbacks(context, keySizeInBits, callbacks));
    } catchAndBridge(outError);
}

CBLKeyPair* CBLKeyPair_CreateWithPrivateKeyData(FLSlice privateKeyData,
                                                FLSlice passwordOrNull,
                                                CBLError* _cbl_nullable outError) noexcept {
    try {
        return retain(CBLKeyPair::CreateKeyPairWithPrivateKeyData(privateKeyData, passwordOrNull));
    } catchAndBridge(outError);
}

// CBLPrivate.h
CBLKeyPair* CBLKeyPair_GenerateRSAKeyPair(FLSlice passwordOrNull, CBLError* outError) noexcept {
    try {
        return retain(CBLKeyPair::GenerateRSAKeyPair(passwordOrNull));
    } catchAndBridge(outError);
}

// CBLPrivate.h
CBLKeyPair* CBLKeyPair_PublicKeyFromData(FLSlice data,
                                         CBLError* _cbl_nullable outError) noexcept {
    try {
        return retain(CBLKeyPair::PublicKeyFromData(data));
    } catchAndBridge(outError);
}

FLSliceResult CBLKeyPair_PublicKeyDigest(CBLKeyPair* keyPair) noexcept {
    return (FLSliceResult)keyPair->publicKeyDigest();
}

FLSliceResult CBLKeyPair_PublicKeyData(CBLKeyPair* keyPair) noexcept {
    return (FLSliceResult)keyPair->publicKeyData();
}

FLSliceResult CBLKeyPair_PrivateKeyData(CBLKeyPair* keyPair) noexcept {
    return (FLSliceResult)keyPair->privateKeyData();
}

// CBLCert:

CBLCert* CBLCert_CreateWithData(FLSlice certData, CBLError* outError) noexcept {
    try {
        return retain(CBLCert::CreateWithData(certData));
    } catchAndBridge(outError);
}

CBLCert* CBLCert_CertNextInChain(CBLCert* cert) noexcept {
    try {
        return retain(cert->certNextInChain());
    } catchAndBridge(nullptr);
}

FLSliceResult CBLCert_Data(CBLCert* cert, bool pemEncoded) noexcept {
    try {
        return (FLSliceResult)cert->data(pemEncoded);
    } catchAndBridge(nullptr);
}

FLSliceResult CBLCert_SubjectName(CBLCert* cert) noexcept {
    try {
        return (FLSliceResult)cert->subjectName();
    } catchAndBridge(nullptr);
}

FLSliceResult CBLCert_SubjectNameComponent(CBLCert* cert, FLString attributeKey) noexcept {
    try {
        return (FLSliceResult)cert->subjectNameComponent(attributeKey);
    } catchAndBridge(nullptr);
}

void CBLCert_getValidTimespan(CBLCert* cert,
                              CBLTimestamp* outCreated,
                              CBLTimestamp* outExpires) noexcept {
    try {
        cert->getValidTimespan(outCreated, outExpires);
    } catchAndWarnNoReturn();
}

CBLKeyPair* CBLCert_PublicKey(CBLCert* cert) noexcept {
    try {
        return retain(cert->publicKey());
    } catchAndBridge(nullptr);
}

// CBLTLSIdentity

CBLTLSIdentity* CBLTLSIdentity_CreateIdentityWithKeyPair(CBLKeyUsages usages,
                                                         CBLKeyPair* keypair,
                                                         FLDict attributes,
                                                         CBLTimestamp expiration,
                                                         CBLError* outError) noexcept {
    try {
        return retain(CBLTLSIdentity::CreateIdentity(usages,
                                                     keypair,
                                                     attributes,
                                                     expiration));
    } catchAndBridge(outError);
}

CBLTLSIdentity* CBLTLSIdentity_IdentityWithKeyPairAndCerts(CBLKeyPair* keypair,
                                                           CBLCert* cert,
                                                           CBLError*  outError) noexcept {
    try {
        return retain(CBLTLSIdentity::IdentityWithKeyPairAndCerts(keypair, cert));
    } catchAndBridge(outError);
}

CBLCert* CBLTLSIdentity_Certificates(CBLTLSIdentity* tlsID) noexcept {
    return tlsID->certificates();
}

CBLTimestamp CBLTLSIdentity_Expiration(CBLTLSIdentity* tlsID) noexcept {
    return tlsID->expiration();
}

CBLTLSIdentity* CBLTLSIdentity_CreateIdentity(CBLKeyUsages usages,
                                              FLDict attrs,
                                              CBLTimestamp exp,
                                              FLString label,
                                              CBLError* outError) noexcept {
    try {
        return retain(CBLTLSIdentity::CreateIdentity(usages, attrs, exp, label));
    } catchAndBridge(outError);
}

#if !defined(__linux__) && !defined(__ANDROID__)

bool CBLTLSIdentity_DeleteIdentityWithLabel(FLString label,
                                            CBLError* _cbl_nullable outError) noexcept {
    try {
        return CBLTLSIdentity::DeleteIdentityWithLabel(label);
    } catchAndBridge(outError);
}

CBLTLSIdentity* _cbl_nullable CBLTLSIdentity_IdentityWithLabel(FLString label,
                                                               CBLError* outError) noexcept {
    try {
        return retain(CBLTLSIdentity::IdentityWithLabel(label));
    } catchAndBridge(outError);
}

CBLTLSIdentity* _cbl_nullable CBLTLSIdentity_IdentityWithCerts(CBLCert* cert,
                                                               CBLError* _cbl_nullable outError) noexcept {
    try {
        return retain(CBLTLSIdentity::IdentityWithCerts(cert));
    } catchAndBridge(outError);
}

#endif // #if !defined(__linux__) && !defined(__ANDROID__)

#endif // #ifdef COUCHBASE_ENTERPRISE
