//
// CBLReplicatorConfig.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

#include "CBLReplicator.h"
#include "CBLDatabase_Internal.hh"
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
    virtual C4String remoteDatabaseName() const                 {return nullslice;}
    virtual C4Database* otherLocalDB() const                    {return nullptr;}

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

    private:
        alloc_slice _url;
        C4String _dbName = { };
    };

#ifdef COUCHBASE_ENTERPRISE
    // Concrete Endpoint for local databases
    struct CBLLocalEndpoint : public CBLEndpoint {
        CBLLocalEndpoint(CBLDatabase *db _cbl_nonnull)
        :_db(db)
        { }

        bool valid() const override                             {return true;}
        virtual C4Database* otherLocalDB() const override       {return internal(_db);}

    private:
        Retained<CBLDatabase> _db;
    };
#endif
}


#pragma mark - AUTHENTICATOR:


struct CBLAuthenticator {
    virtual ~CBLAuthenticator()                                 { }
    virtual void writeOptions(Encoder&) =0;
};


namespace cbl_internal {
    // Concrete Authenticator for HTTP Basic auth:
    struct BasicAuthenticator : public CBLAuthenticator {
        BasicAuthenticator(const char *username _cbl_nonnull, const char *password _cbl_nonnull)
        :_username(username)
        ,_password(password)
        { }

        virtual void writeOptions(Encoder &enc) override {
            enc.writeKey(slice(kC4ReplicatorOptionAuthentication));
            enc.beginDict();
            enc[slice(kC4ReplicatorAuthUserName)] = _username;
            enc[slice(kC4ReplicatorAuthPassword)] = _password;
            enc.endDict();
        }

    private:
        alloc_slice _username, _password;
    };

    // Concrete Authenticator for session-cookie auth:
    struct SessionAuthenticator : public CBLAuthenticator {
        SessionAuthenticator(const char *sessionID _cbl_nonnull, const char *cookieName)
        :_sessionID(sessionID)
        ,_cookieName(cookieName ? cookieName : kDefaultCookieName)
        { }

        static constexpr const char* kDefaultCookieName = "SyncGatewaySession";

        virtual void writeOptions(Encoder &enc) override {
            enc.writeKey(slice(kC4ReplicatorOptionCookies));
            enc.writeString(_cookieName + "=" + _sessionID);
        }

    private:
        string _sessionID, _cookieName;
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

        // Writes a LiteCore replicator optionsDict
        void writeOptions(Encoder &enc) const {
            writeOptionalKey(enc, kC4ReplicatorOptionExtraHeaders,  Dict(headers));
            writeOptionalKey(enc, kC4ReplicatorOptionDocIDs,        Array(documentIDs));
            writeOptionalKey(enc, kC4ReplicatorOptionChannels,      Array(channels));
            if (pinnedServerCertificate.buf) {
                enc.writeKey(slice(kC4ReplicatorOptionPinnedServerCert));
                enc.writeData(pinnedServerCertificate);
            }
            if (authenticator)
                authenticator->writeOptions(enc);
        }

        ReplicatorConfiguration(const ReplicatorConfiguration&) =delete;
        ReplicatorConfiguration& operator=(const ReplicatorConfiguration&) =delete;
    };
}
