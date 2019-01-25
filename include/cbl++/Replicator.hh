//
//  Replicator.hh
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

namespace cbl {

    class Endpoint {
    public:
        void setURL(const char *url _cbl_nonnull)   {_ref = cblendpoint_newWithURL(url);}
#ifdef COUCHBASE_ENTERPRISE
        void setLocalDB(Database db)                {_ref = cblendpoint_newWithLocalDB(db.ref());}
#endif
        ~Endpoint()                                 {cblendpoint_free(_ref);}
        CBLEndpoint* ref() const                    {return _ref;}
    private:
        CBLEndpoint* _ref {nullptr};
    };


    class Authenticator {
    public:
        void setBasic(const char *username _cbl_nonnull,
                      const char *password _cbl_nonnull)
                                                    {_ref = cblauth_newBasic(username, password);}
        ~Authenticator()                            {cblauth_free(_ref);}
        CBLAuthenticator* ref() const               {return _ref;}
    private:
        CBLAuthenticator* _ref {nullptr};
    };


    using ReplicationFilter = std::function<bool(Document, bool isDeleted)>;


    struct ReplicatorConfiguration {
        ReplicatorConfiguration(Database db)
        :database(db)
        { }

        Database const database;
        Endpoint endpoint;
        CBLReplicatorType replicatorType    = kCBLReplicatorTypePushAndPull;
        bool continuous                     = false;
        Authenticator authenticator;
        fleece::alloc_slice pinnedServerCertificate;
        fleece::MutableDict headers         = fleece::MutableDict::newDict();
        fleece::MutableArray channels       = fleece::MutableArray::newArray();
        fleece::MutableArray documentIDs    = fleece::MutableArray::newArray();
        ReplicationFilter pushFilter;
        ReplicationFilter pullFilter;

        operator CBLReplicatorConfiguration() const {
            return {
                database.ref(),
                endpoint.ref(),
                replicatorType,
                continuous,
                authenticator.ref(),
                pinnedServerCertificate,
                headers,
                channels,
                documentIDs
            };
        }
    };

    class Replicator : private RefCounted {
    public:
        Replicator(const ReplicatorConfiguration &config) {
            CBLError error;
            CBLReplicatorConfiguration c_config = config;
#if 0 // TODO
            if (config.pushFilter) {
                c_config.pushFilter = [](void *context, CBLDocument* doc, bool isDeleted) {
                    return ((Replicator*)context)->_pushFilter(Document(doc), isDeleted);
                };
            }
            if (config.pullFilter) {
                c_config.pullFilter = [](void *context, CBLDocument* doc, bool isDeleted) {
                    return ((Replicator*)context)->_pullFilter(Document(doc), isDeleted);
                };
            }
            c_config.filterContext = this;
#endif
            _ref = (CBLRefCounted*) cbl_repl_new(&c_config, &error);
            check(_ref, error);
        }

        void start()                {cbl_repl_start(ref());}
        void stop()                 {cbl_repl_stop(ref());}

        void resetCheckpoint()      {cbl_repl_resetCheckpoint(ref());}

        CBLReplicatorStatus status() const  {return cbl_repl_status(ref());}

        using Listener = cbl::ListenerToken<Replicator, const CBLReplicatorStatus&>;

        [[nodiscard]] Listener addListener(Listener::Callback f) {
            auto l = Listener(f);
            l.setToken( cbl_repl_addListener(ref(), &_callListener, l.context()) );
            return l;
        }

    private:
        static void _callListener(void *context, CBLReplicator *repl,
                                  const CBLReplicatorStatus *status)
        {
            Listener::call(context, Replicator(repl), *status);
        }

        CBL_REFCOUNTED_BOILERPLATE(Replicator, RefCounted, CBLReplicator)
    };
}
