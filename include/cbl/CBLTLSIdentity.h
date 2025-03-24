//
//  CBLTLSIdentity.h
//  CBL_C
//
//  Created by Pasin Suriyentrakorn on 2/21/25.
//  Copyright © 2025 Couchbase. All rights reserved.
//

#pragma once

#include "CBLBase.h"

#ifdef COUCHBASE_ENTERPRISE

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#endif

CBL_CAPI_BEGIN

// Certificate Attribute Keys:
CBL_PUBLIC extern const FLString kCBLCertAttrKeyCommonName;        // "CN",              e.g. "Jane Doe", (or "jane.example.com")
CBL_PUBLIC extern const FLString kCBLCertAttrKeyPseudonym;         // "pseudonym",       e.g. "plainjane837"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyGivenName;         // "GN",              e.g. "Jane"
CBL_PUBLIC extern const FLString kCBLCertAttrKeySurname;           // "SN",              e.g. "Doe"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyOrganization;      // "O",               e.g. "Example Corp."
CBL_PUBLIC extern const FLString kCBLCertAttrKeyOrganizationUnit;  // "OU",              e.g. "Marketing"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyPostalAddress;     // "postalAddress",   e.g. "123 Example Blvd #2A"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyLocality;          // "locality",        e.g. "Boston"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyPostalCode;        // "postalCode",      e.g. "02134"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyStateOrProvince;   // "ST",              e.g. "Massachusetts" (or "Quebec", ...)
CBL_PUBLIC extern const FLString kCBLCertAttrKeyCountry;           // "C",               e.g. "us" (2-letter ISO country code)

// Certificate Subject Alternative Name attribute Keys:
CBL_PUBLIC extern const FLString kCBLCertAttrKeyEmailAddress;      // "rfc822Name",      e.g. "jane@example.com"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyHostname;          // "dNSName",         e.g. "www.example.com"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyURL;               // "uniformResourceIdentifier", e.g. "https://example.com/jane"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyIPAddress;         // "iPAddress",       e.g. An IP Address in binary format e.g. "\x0A\x00\x01\x01"
CBL_PUBLIC extern const FLString kCBLCertAttrKeyRegisteredID;      // "registeredID",    e.g. A domain specific identifier.

/** Digest algorithms to be used when generating signatures. */
typedef CBL_ENUM(int, CBLSignatureDigestAlgorithm) {
    kCBLSignatureDigestNone = 0,   ///< No digest, just direct signature of input data.
    kCBLSignatureDigestSHA1 = 4,   ///< SHA-1 message digest.
    kCBLSignatureDigestSHA224,     ///< SHA-224 message digest.
    kCBLSignatureDigestSHA256,     ///< SHA-256 message digest.
    kCBLSignatureDigestSHA384,     ///< SHA-384 message digest.
    kCBLSignatureDigestSHA512,     ///< SHA-512 message digest.
    kCBLSignatureDigestRIPEMD160,  ///< RIPEMD-160 message digest.
};

/** Callbacks provided to create a keypair to perform the crypto operations necessary for TLS.
    In general, these crypto operations are performed inside a secure keystore available on the platform. */
typedef struct CBLKeyPairCallbacks {
    /** Provides the public key's raw data, as an ASN.1 DER sequence of [modulus, exponent].
        @param context  The context given to CBLKeyPair_RSAKeyPairWithCallbacks.
        @param output  Where to copy the key data.
        @param outputMaxLen  Maximum length of output that can be written.
        @param outputLen  Store the length of the output here before returning.
        @return True on success, false on failure. */
    bool (*publicKeyData)(void* context, void* output, size_t outputMaxLen, size_t* outputLen);
    
    /** Decrypts data using the private key.
        @param context  The context given to CBLKeyPair_RSAKeyPairWithCallbacks.
        @param input  The encrypted data (size is always equal to the key size.)
        @param output  Where to write the decrypted data.
        @param outputMaxLen  Maximum length of output that can be written.
        @param outputLen  Store the length of the output here before returning.
        @return True on success, false on failure. */
    bool (*decrypt)(void* context, FLSlice input, void* output, size_t outputMaxLen, size_t* outputLen);
    
    /** Uses the private key to generate a signature of input data.
        @param context  The context given to CBLKeyPair_RSAKeyPairWithCallbacks.
        @param digestAlgorithm  Indicates what type of digest to create the signature from.
        @param inputData  The data to be signed.
        @param outSignature  Write the signature here; length must be equal to the key size.
        @return True on success, false on failure.
        @note The data in inputData is already hashed and DOES NOT need to be hashed by the caller.  The
              algorithm is provided as a reference for what was used to perform the hashing. */
    bool (*sign)(void* context, CBLSignatureDigestAlgorithm digestAlgorithm, FLSlice inputData, void* outSignature);
    
    /** Called when the CBLKeyPair is released and the callback is no longer needed, so that
        your code can free any associated resources. (This callback is optionaly and may be NULL.)
        @param context The context given to CBLKeyPair_RSAKeyPairWithCallbacks. */
    void (*_cbl_nullable free)(void* context);
} CBLKeyPairCallbacks;

/** An opaque object representing the KeyPair. */
typedef struct CBLKeyPair CBLKeyPair;
CBL_REFCOUNTED(CBLKeyPair*, KeyPair);

/** An opaque object representing the KeyCert. */
typedef struct CBLCert CBLCert;
CBL_REFCOUNTED(CBLCert*, Cert);

/** An opaque object representing the TLSIdentity. */
typedef struct CBLTLSIdentity CBLTLSIdentity;
CBL_REFCOUNTED(CBLTLSIdentity*, TLSIdentity);

/** Returns a KeyPair from external key provided as the context
 @note You are responsible for releasing the returned KeyPair */
_cbl_warn_unused
CBLKeyPair* _cbl_nullable CBLKeyPair_RSAKeyPairWithCallbacks(void* context,
                                                             size_t keySizeInBits,
                                                             CBLKeyPairCallbacks callbacks,
                                                             CBLError* _cbl_nullable outError) CBLAPI;

/** Returns a KeyPair from private key data.
 @note You are responsible for releasig the returned KeyPair. */
_cbl_warn_unused
CBLKeyPair* _cbl_nullable CBLKeyPair_RSAKeyPairWithPrivateKeyData(FLSlice privateKeyData,
                                                                  FLSlice passwordOrNull,
                                                                  CBLError* _cbl_nullable outError) CBLAPI;


/** Returns a hex digest of the public key.
    @note You are responsible for releasing the returned data. */
_cbl_warn_unused
FLSliceResult CBLKeyPair_PublicKeyDigest(CBLKeyPair*) CBLAPI;

/** Returns the public key data.
    @note You are responsible for releasing the returned data. */
_cbl_warn_unused
FLSliceResult CBLKeyPair_PublicKeyData(CBLKeyPair*) CBLAPI;

/** Returns the private key data, if the private key is known and its data is accessible.
    @note Persistent private keys in the secure store generally don't have accessible data.
    @note You are responsible for releasing the returned data. */
_cbl_warn_unused
FLSliceResult CBLKeyPair_PrivateKeyData(CBLKeyPair*) CBLAPI;

/** Instantiates a CBLCert from X.509 certificate data in DER or PEM form.
    @note PEM data might consist of a series of certificates. If so, the returned CBLCert
          will represent only the first, and you can iterate over the next by calling \ref CBLCert_NextInChain.
    @note You are responsible for releasing the returned reference. */
_cbl_warn_unused
CBLCert* _cbl_nullable CBLCert_CertFromData(FLSlice certData, CBLError* _cbl_nullable outError) CBLAPI;

_cbl_warn_unused
CBLCert* _cbl_nullable CBLCert_CertNextInChain(CBLCert* cert) CBLAPI;

/** Returns the encoded X.509 data in DER (binary) or PEM (ASCII) form.
    @warning DER format can only encode a single certificate, so if this CBLCert includes multiple certificates, use PEM format to preserve them.
    @note You are responsible for releasing the returned data. */
_cbl_warn_unused
FLSliceResult CBLCert_Data(CBLCert*, bool pemEncoded) CBLAPI;

/** Returns the cert's Subject Name, which identifies the cert's owner.
    This is an X.509 structured string consisting of "KEY=VALUE" pairs separated by commas,
    where the keys are attribute names. (Commas in values are backslash-escaped.)
    @note Rather than parsing this yourself, use \ref CBLCert_SubjectNameComponent.
    @note You are responsible for releasing the returned data. */
_cbl_warn_unused
FLSliceResult CBLCert_SubjectName(CBLCert*) CBLAPI;

/** Returns one component of a cert's subject name, given the attribute key.
    @note If there are multiple names with this ID, only the first is returned.
    @note You are responsible for releasing the returned string. */
_cbl_warn_unused
FLSliceResult CBLCert_SubjectNameComponent(CBLCert*, FLString attributeKey) CBLAPI;

/** Returns the time range during which a (signed) certificate is valid.
    @param cert  The signed certificate.
    @param outCreated  On return, the date/time the cert became valid (was signed).
    @param outExpires  On return, the date/time at which the certificate expires. */
void CBLCert_getValidTimespan(CBLCert* cert,
                             CBLTimestamp* _cbl_nullable outCreated,
                             CBLTimestamp* _cbl_nullable outExpires) CBLAPI;

/** Returns a certificate's public key.
    @note You are responsible for releasing the returned key reference. */
_cbl_warn_unused
CBLKeyPair* CBLCert_PublicKey(CBLCert*) CBLAPI;

/** Returns the certificate chain of the given TLS identity. */
_cbl_warn_unused
CBLCert* CBLTLSIdentity_Certificates(CBLTLSIdentity*) CBLAPI;

/** Returns the date/time at which the first certificate in the chain expires. */
CBLTimestamp CBLTLSIdentity_Expiration(CBLTLSIdentity*) CBLAPI;

/**
 Creates a self-signed TLS identity with the given RSA keypair and certificate attributes.
 @Note The Common Name (kCBLCertAttrKeyCommonName) attribute is required.
 @Note You are responsible for releasig the returned reference. */
_cbl_warn_unused
CBLTLSIdentity* _cbl_nullable CBLTLSIdentity_SelfSignedCertIdentity(bool server,
                                                                    CBLKeyPair* keypair,
                                                                    FLDict attributes,
                                                                    CBLTimestamp expiration,
                                                                    CBLError* _cbl_nullable outError) CBLAPI;

#if !defined(__linux__) && !defined(__ANDROID__)

/**
 Creates a self-signed TLS identity with the given RSA keypair and certificate attributes. If the persietent label is specified,
 The identity will be persisted in the platform's keystore (Keychain for Apple or x509 Key/Certificate Stores for Windows)
 @Note The Common Name (kCBLCertAttrKeyCommonName) attribute is required.
 @Note You are responsible for releasing the returned reference.
 */
_cbl_warn_unused
CBLTLSIdentity* _cbl_nullable CBLTLSIdentity_SelfSignedCertIdentityWithLabel(bool server,
                                                                    FLString persistentLabel,
                                                                    FLDict attributes,
                                                                    CBLTimestamp expiration,
                                                                    CBLError* _cbl_nullable outError) CBLAPI;

/** Deletes the identity from the platform's keystore (Keychain for Apple or x509 Key/Certificate Stores for Windows) witih the given persistent label. */
bool CBLTLSIdentity_DeleteIdentityWithLabel(FLString persistentLabel,
                                            CBLError* _cbl_nullable outError) CBLAPI;

_cbl_warn_unused
/** Creates a TLS identity from the identity in the platfrom's keystore (Keychain for Apple or x509 Key/Certificate Stores for Windows)
    with the given persistent label.
 @note You are responsible for releasing the returned reference. */
CBLTLSIdentity* _cbl_nullable CBLTLSIdentity_IdentityWithLabel(FLString persistentLabel,
                                                               CBLError* _cbl_nullable outError) CBLAPI;

#endif //#if !defined(__linux__) && !defined(__ANDROID__)

/** Create a TLS identity with the given RSA keypair and certificates. */
_cbl_warn_unused
CBLTLSIdentity* _cbl_nullable CBLTLSIdentity_IdentityWithKeyPairAndCerts(CBLKeyPair* keypair,
                                                                         CBLCert* cert,
                                                                         CBLError* _cbl_nullable outError) CBLAPI;

#ifdef __OBJC__

/** Creates a TLS identity with the given SecIdentity object.
 @note You are responsible for releasing the returned reference. */
CBLTLSIdentity* _cbl_nullable CBLTLSIdentity_IdentityWithSecIdentity(SecIdentityRef secIdentity,
                                                                     NSArray* _cbl_nullable certs,
                                                                     CBLError* _cbl_nullable outError) CBLAPI;

#endif //#ifdef __OBJC__

CBL_CAPI_END

#endif // #ifdef COUCHBASE_ENTERPRISE
