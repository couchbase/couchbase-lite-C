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


#pragma mark - ENDPOINT


struct CBLEndpoint {
    virtual ~CBLEndpoint()                                      { }
    virtual bool valid() const =0;
    const C4Address& remoteAddress() const                      {return _address;}
    virtual C4String remoteDatabaseName() const =0;
#ifdef COUCHBASE_ENTERPRISE
    virtual C4Database* otherLocalDB() const                    {return nullptr;}
#endif

protected:
    C4Address _address = { };
};


namespace cbl_internal {
    // Concrete Endpoint for remote URLs
    struct CBLURLEndpoint : public CBLEndpoint {
        CBLURLEndpoint(fleece::slice url)
        :_url(url)
        {
            if (!c4address_fromURL(_url, &_address, &_dbName))
                _dbName = fleece::nullslice; // mark as invalid
        }

        bool valid() const override                             {return _dbName != fleece::nullslice;}
        C4String remoteDatabaseName() const override            {return _dbName;}

    private:
        fleece::alloc_slice _url;
        C4String _dbName = { };
    };

#ifdef COUCHBASE_ENTERPRISE
    // Concrete Endpoint for local databases
    struct CBLLocalEndpoint : public CBLEndpoint {
        CBLLocalEndpoint(CBLDatabase *db _cbl_nonnull)
        :_db(db)
        { }

        bool valid() const override                             {return true;}
        virtual C4String remoteDatabaseName() const override    {return fleece::nullslice;}
        virtual C4Database* otherLocalDB() const override       {return _db->_getC4Database();}

    private:
        fleece::Retained<CBLDatabase> _db;
    };
#endif
}


#pragma mark - AUTHENTICATOR:


struct CBLAuthenticator {
protected:
    using slice = fleece::slice;
    using Encoder = fleece::Encoder;
    using alloc_slice = fleece::alloc_slice;
    using string = std::string;

public:
    virtual ~CBLAuthenticator()                                 { }
    virtual void writeOptions(Encoder&) =0;
};


namespace cbl_internal {
    // Concrete Authenticator for HTTP Basic auth:
    struct BasicAuthenticator : public CBLAuthenticator {
        BasicAuthenticator(slice username, slice password)
        :_username(username)
        ,_password(password)
        { }

        virtual void writeOptions(Encoder &enc) override {
            enc.writeKey(slice(kC4ReplicatorOptionAuthentication));
            enc.beginDict();
            enc[slice(kC4ReplicatorAuthType)] = kC4AuthTypeBasic;
            enc[slice(kC4ReplicatorAuthUserName)] = _username;
            enc[slice(kC4ReplicatorAuthPassword)] = _password;
            enc.endDict();
        }

    private:
        alloc_slice _username, _password;
    };

    // Concrete Authenticator for session-cookie auth:
    struct SessionAuthenticator : public CBLAuthenticator {
        SessionAuthenticator(slice sessionID, slice cookieName)
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
    class ReplicatorConfiguration : public CBLReplicatorConfiguration {
        using Encoder = fleece::Encoder;
        using Dict = fleece::Dict;
        using slice = fleece::slice;
        using Array = fleece::Array;

    public:
        ReplicatorConfiguration(const CBLReplicatorConfiguration &conf) {
            *(CBLReplicatorConfiguration*)this = conf;
            retain(database);
            headers = FLDict_MutableCopy(headers, kFLDeepCopyImmutables);
            channels = FLArray_MutableCopy(channels, kFLDeepCopyImmutables);
            documentIDs = FLArray_MutableCopy(documentIDs, kFLDeepCopyImmutables);
            pinnedServerCertificate = (_pinnedServerCert = pinnedServerCertificate);
            trustedRootCertificates = (_trustedRootCerts = trustedRootCertificates);
            if (proxy) {
                _proxy = *proxy;
                proxy = &_proxy;
                _proxy.hostname = copyString(_proxy.hostname, _proxyHostname);
                _proxy.username = copyString(_proxy.username, _proxyUsername);
                _proxy.password = copyString(_proxy.password, _proxyPassword);
            }
        }


        ~ReplicatorConfiguration() {
            release(database);
            FLDict_Release(headers);
            FLArray_Release(channels);
            FLArray_Release(documentIDs);
        }


        bool validate(CBLError *outError) const {
            slice problem;
            if (!database || !endpoint || replicatorType > kCBLReplicatorTypePull)
                problem = slice("Invalid replicator config: missing endpoints or bad type");
            else if (!endpoint->valid())
                problem = slice("Invalid endpoint");
            else if (proxy && (proxy->type > kCBLProxyHTTPS || !proxy->hostname || !proxy->port))
                problem = slice("Invalid replicator proxy settings");

            if (!problem)
                return true;
            c4error_return(LiteCoreDomain, kC4ErrorInvalidParameter, problem, internal(outError));
            return false;
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
            if (trustedRootCertificates.buf) {
                enc.writeKey(slice(kC4ReplicatorOptionRootCerts));
                enc.writeData(trustedRootCertificates);
            }
            if (authenticator)
                authenticator->writeOptions(enc);
            if (proxy) {
                static constexpr const char* kProxyTypeIDs[] = {kC4ProxyTypeHTTP,
                                                                kC4ProxyTypeHTTPS};
                enc.writeKey(slice(kC4ReplicatorOptionProxyServer));
                enc.beginDict();
                enc[slice(kC4ReplicatorProxyType)] = kProxyTypeIDs[proxy->type];
                enc[slice(kC4ReplicatorProxyHost)] = proxy->hostname;
                enc[slice(kC4ReplicatorProxyPort)] = proxy->port;
                if (proxy->username) {
                    enc.writeKey(slice(kC4ReplicatorProxyAuth));
                    enc.beginDict();
                    enc[slice(kC4ReplicatorAuthUserName)] = proxy->username;
                    enc[slice(kC4ReplicatorAuthPassword)] = proxy->password;
                    enc.endDict();
                }
                enc.endDict();
            }
        }


        ReplicatorConfiguration(const ReplicatorConfiguration&) =delete;
        ReplicatorConfiguration& operator=(const ReplicatorConfiguration&) =delete;

    private:
        using string = std::string;
        using alloc_slice = fleece::alloc_slice;

        static const char* copyString(const char *cstr, string &str) {
            if (!cstr) return nullptr;
            str = cstr;
            return str.c_str();
        }


        alloc_slice _pinnedServerCert, _trustedRootCerts;
        CBLProxySettings _proxy;
        string _proxyHostname, _proxyUsername, _proxyPassword;
    };
}
