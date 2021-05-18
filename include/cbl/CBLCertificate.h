//
// CBLCertificate.h
//
// Copyright (c) 2020 Couchbase, Inc All rights reserved.
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
#include "CBLBase.h"

#ifndef COUCHBASE_ENTERPRISE
#error This API is part of Couchbase Lite Enterprise Edition only.
#endif


CBL_CAPI_BEGIN

/** \defgroup certificates   TLS Certificates And Key-Pairs
    @{ */


/** Represents an X.509 certificate, used for TLS server or client authentication. */
typedef struct CBLCertificate CBLCertificate;

/** Represents a combination of an X.509 certificate and the matching private key. */
typedef struct CBLTLSIdentity CBLTLSIdentity;


/** Creates a certificate object given a pregenerated X.509 certificate.
    @param certData  The X.509 certificate encoded in PEM or DER format.
    @param outError  On failure, an error will be stored here.
    @return  A new CBLCertificate object. */
_cbl_warn_unused
CBLCertificate* CBLCertificate_NewFromData(FLSlice certData,
                                           CBLError* _cbl_nullable outError) CBLAPI;

/** Returns the certificate's data, encoded in PEM (ASCII) format. */
FLSliceResult CBLCertificate_PEMData(CBLCertificate*) CBLAPI;

/** Returns the certificate's data, encoded in DER (binary) format. */
FLSliceResult CBLCertificate_DERData(CBLCertificate*) CBLAPI;

/** If this certificate is part of a chain, returns the next certificate in the chain.
    PEM data may contain multiple certificates. If you give such data to
    \ref CBLCertificate_NewFromData, use this function to access the certificates past the first.
    \warning This returns a new object. You are responsible for releasing it. */
_cbl_warn_unused
CBLCertificate* CBLCertificate_NextInChain(CBLCertificate*) CBLAPI;

CBL_REFCOUNTED(CBLCertificate*, Certificate);


/** Creates a TLS identity object given an encoded RSA private key and a certificate.
    @param privateKeyData  RSA private key data, in PKCS#1 or SEC1 DER format.
    @param certificate  An X.509 certificate for the public key
    @param outError  On failure, an error will be stored here.
    @return  A new CBLTLSIdentity object. */
_cbl_warn_unused
CBLTLSIdentity* CBLTLSIdentity_NewFromData(FLSlice privateKeyData,
                                           CBLCertificate *certificate,
                                           CBLError* _cbl_nullable outError) CBLAPI;

/** Generates a new random RSA key-pair, and creates a self-signed certificate from the public key.
    This 'identity' is not useful for any real identification, but can be used with a TLS
    server to provide encryption of the data stream.
    @param outError  On failure, an error will be stored here.
    @return  A new CBLTLSIdentity object. */
_cbl_warn_unused
CBLTLSIdentity* CBLTLSIdentity_GenerateAnonymous(CBLError* _cbl_nullable outError) CBLAPI;

/** Returns the identity's certificate object. */
CBLCertificate* CBLTLSIdentity_GetCertificate(CBLTLSIdentity*) CBLAPI;

/** Returns the encoded form of the identity's private key, in PKCS#1 or SEC1 DER format.
    This can be used together with the certificate's data to re-create the CBLTLSIdentity later.
    \warning This data is highly sensitive, just like a password; it should never be stored where
             anyone else can read it. */
FLSliceResult CBLTLSIdentity_PrivateKeyData(CBLTLSIdentity*) CBLAPI;

CBL_REFCOUNTED(CBLTLSIdentity*, TLSIdentity);


/** @} */

CBL_CAPI_END
