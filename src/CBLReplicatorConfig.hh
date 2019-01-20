//
// CBLReplicatorConfig.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once

#include "CBLReplicator.h"
#include "Internal.hh"
#include "c4.hh"
#include "c4Replicator.h"
#include "c4Private.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <mutex>

using namespace std;
using namespace fleece;


#pragma mark - ENDPOINT


struct CBLEndpoint {
    virtual ~CBLEndpoint()                                      { }
    virtual bool valid() const =0;
    const C4Address& remoteAddress() const                      {return _address;}
    virtual C4String remoteDatabaseName() const =0;
    virtual CBLDatabase* otherLocalDB() const =0;

protected:
    C4Address _address = { };
};


namespace cbl_internal {
    // Concrete Endpoint for remote URLs
    struct CBLURLEndpoint : public CBLEndpoint {
        CBLURLEndpoint(const char *url _cbl_nonnull)
        :_url(url)
        {
            if (!c4address_fromURL(_url, &_address, &_dbName))
                _dbName = nullslice;
        }

        bool valid() const override                             {return _dbName != nullslice;}
        C4String remoteDatabaseName() const override            {return _dbName;}
        virtual CBLDatabase* otherLocalDB() const override      {return nullptr;}

        alloc_slice _url;
        C4String _dbName = { };
    };
}


#pragma mark - AUTHENTICATOR:


struct CBLAuthenticator {
    virtual ~CBLAuthenticator()                                 { }
    virtual void writeAuthDict(Encoder&) =0;
};


namespace cbl_internal {
    // Concrete Authenticator for HTTP Basic auth:
    struct CBLBasicAuthenticator : public CBLAuthenticator {
        CBLBasicAuthenticator(const char *username _cbl_nonnull, const char *password _cbl_nonnull)
        :_username(username)
        ,_password(password)
        { }

        virtual void writeAuthDict(Encoder &enc) override {
            enc[slice(kC4ReplicatorAuthUserName)] = _username;
            enc[slice(kC4ReplicatorAuthPassword)] = _password;
        }

    private:
        alloc_slice _username, _password;
    };
}

#pragma mark - CONFIGURATION:


namespace cbl_internal {
    // Managed config object that retains/releases its properties.
    struct ReplicatorConfiguration : public CBLReplicatorConfiguration {
        ReplicatorConfiguration(const CBLReplicatorConfiguration &conf) {
            *(CBLReplicatorConfiguration*)this = conf;
            retain(database);
            headers = FLDict_MutableCopy(headers, kFLDeepCopyImmutables);
            channels = FLArray_MutableCopy(channels, kFLDeepCopyImmutables);
            documentIDs = FLArray_MutableCopy(documentIDs, kFLDeepCopyImmutables);
        }

        ~ReplicatorConfiguration() {
            release(database);
            FLDict_Release(headers);
            FLArray_Release(channels);
            FLArray_Release(documentIDs);
        }

        bool validate(CBLError *outError) const {
            if (!database || !endpoint || replicatorType > kCBLReplicatorTypePull) {
                c4error_return(LiteCoreDomain, kC4ErrorInvalidParameter,
                               "Invalid replicator config"_sl, internal(outError));
                return false;
            }
            return true;
        }

        void writeOptions(Encoder &enc) const {
            writeOptionalKey(enc, kC4ReplicatorOptionExtraHeaders,  Dict(headers));
            writeOptionalKey(enc, kC4ReplicatorOptionDocIDs,        Array(documentIDs));
            writeOptionalKey(enc, kC4ReplicatorOptionChannels,      Array(channels));
            if (pinnedServerCertificate.buf) {
                enc.writeKey(slice(kC4ReplicatorOptionPinnedServerCert));
                enc.writeData(pinnedServerCertificate);
            }
            if (authenticator) {
                enc.writeKey(slice(kC4ReplicatorOptionAuthentication));
                enc.beginDict();
                authenticator->writeAuthDict(enc);
                enc.endDict();
            }
        }

        ReplicatorConfiguration(const ReplicatorConfiguration&) =delete;
        ReplicatorConfiguration& operator=(const ReplicatorConfiguration&) =delete;
    };
}
