//
// CBLCertificate_Internal.hh
//
// Copyright © 2021 Couchbase. All rights reserved.
//

#pragma once
#include "CBLCertificate.h"
#include "Internal.hh"
#include "c4Certificate.hh"

CBL_ASSUME_NONNULL_BEGIN


/** Represents an X.509 certificate, identifying a TLS client or server.
    The certificate contains a public key, and identification data like a name, email address or
    server URL. It's usually signed by a higher-level certificate authority, acting as proof that
    the authority vouches for the identification; but it can instead be self-signed, in which case
    it's valid only as a public key. */
struct CBLCertificate : public CBLRefCounted {
public:
    static Retained<CBLCertificate> fromData(slice certData) {
        return new CBLCertificate(C4Cert::fromData(certData));
    }

    alloc_slice PEMData() const         {return _c4cert->getData(true);}
    alloc_slice DERData() const         {return _c4cert->getData(false);}

    Retained<CBLCertificate> nextInChain() const {
        if (auto next = _c4cert->getNextInChain(); next)
            return new CBLCertificate(next);
        else
            return nullptr;
    }

protected:
    friend struct CBLTLSIdentity;
    friend struct CBLURLEndpointListener;
    explicit CBLCertificate(C4Cert *c4cert)            :_c4cert(c4cert) { }
    explicit CBLCertificate(Retained<C4Cert> &&c4cert) :_c4cert(std::move(c4cert)) { }
    C4Cert* c4Cert() const              {return _c4cert;}

private:
    Retained<C4Cert> _c4cert;
};



/** A combination of an RSA key-pair and an X.509 certificate with the matching public key;
    used for authentication as a TLS server or client.

    The private key of the pair acts as the secret credential to prove ownership of the identity
    expressed by the certificate. */
struct CBLTLSIdentity : public CBLRefCounted {
public:
    static Retained<CBLTLSIdentity> fromPrivateKeyData(slice privateKeyData,
                                                       CBLCertificate *cert)
    {
        return new CBLTLSIdentity(C4KeyPair::fromPrivateKeyData(privateKeyData, fleece::nullslice), cert);
    }

    static Retained<CBLTLSIdentity> generateAnonymous() {
        auto keyPair = C4KeyPair::generate(kC4RSA, 2048, false);
        auto cert = C4Cert::createRequest({{kC4Cert_CommonName, slice("anonymous")}},
                                          kC4CertUsage_TLSServer, keyPair);
        cert = cert->signRequest(kDefaultCertIssuerParameters, keyPair, nullptr);
        return new CBLTLSIdentity(std::move(keyPair), new CBLCertificate(std::move(cert)));
    }

    alloc_slice privateKeyData() const  {return _c4KeyPair->getPrivateKeyData();}

    CBLCertificate* certificate() const {return _cert;}

protected:
    friend struct CBLURLEndpointListener;
    C4KeyPair* c4KeyPair() const    {return _c4KeyPair;}
    C4Cert* c4Cert() const          {return _cert->c4Cert();}

private:
    CBLTLSIdentity(Retained<C4KeyPair> &&kp, CBLCertificate *cert)
    :_c4KeyPair(std::move(kp))
    ,_cert(cert)
    {
        auto certKey = cert->c4Cert()->getPublicKey()->getPublicKeyData();
        auto pubKey = _c4KeyPair->getPublicKeyData();
        if (certKey != pubKey)
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "Cert does not match key-pair");
    }

    Retained<C4KeyPair>         _c4KeyPair;
    Retained<CBLCertificate>    _cert;
};


CBL_ASSUME_NONNULL_END
