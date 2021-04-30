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
#include "cbl++/Document.hh"
#include "cbl/CBLReplicator.h"
#include <functional>
#include <string>
#include <vector>

// PLEASE NOTE: This C++ wrapper API is provided as a convenience only.
// It is not considered part of the official Couchbase Lite API.

CBL_ASSUME_NONNULL_BEGIN

namespace cbl {

    class Endpoint {
    public:
        void setURL(slice url)                      {_ref = CBLEndpoint_NewWithURL(url);}
#ifdef COUCHBASE_ENTERPRISE
        void setLocalDB(Database db)                {_ref = CBLEndpoint_NewWithLocalDB(db.ref());}
#endif
        ~Endpoint()                                 {CBLEndpoint_Free(_ref);}
        CBLEndpoint* ref() const                    {return _ref;}
    private:
        CBLEndpoint* _cbl_nullable _ref {nullptr};
    };


    class Authenticator {
    public:
        void setBasic(slice username,
                      slice password)
                                                    {_ref = CBLAuth_NewPassword(username, password);}

        void setSession(slice sessionId, slice cookieName) {
          _ref = CBLAuth_NewSession(sessionId, cookieName);
        }
        ~Authenticator()                            {CBLAuth_Free(_ref);}
        CBLAuthenticator* ref() const               {return _ref;}
    private:
        CBLAuthenticator* _cbl_nullable _ref {nullptr};
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
        
        unsigned maxAttempts                = 0;
        unsigned maxAttemptWaitTime         = 0;
        
        unsigned heartbeat                  = 0;

        Authenticator authenticator;
        CBLProxySettings* proxy             = nullptr;
        fleece::MutableDict headers         = fleece::MutableDict::newDict();

        std::string pinnedServerCertificate;
        std::string trustedRootCertificates;

        fleece::MutableArray channels       = fleece::MutableArray::newArray();
        fleece::MutableArray documentIDs    = fleece::MutableArray::newArray();

        ReplicationFilter pushFilter;
        ReplicationFilter pullFilter;
        
        operator CBLReplicatorConfiguration() const {
            CBLReplicatorConfiguration conf = {};
            conf.database = database.ref();
            conf.endpoint = endpoint.ref();
            conf.replicatorType = replicatorType;
            conf.continuous = continuous;
            conf.maxAttempts = maxAttempts;
            conf.maxAttemptWaitTime = maxAttemptWaitTime;
            conf.heartbeat = heartbeat;
            conf.authenticator = authenticator.ref();
            conf.proxy = proxy;
            if (!headers.empty())
                conf.headers = headers;
            conf.pinnedServerCertificate = slice(pinnedServerCertificate);
            conf.trustedRootCertificates = slice(trustedRootCertificates);
            if (!channels.empty())
                conf.channels = channels;
            if (!documentIDs.empty())
                conf.documentIDs = documentIDs;
            return conf;
        }
    };

    class Replicator : private RefCounted {
    public:
        Replicator(const ReplicatorConfiguration &config) {
            CBLError error;
            CBLReplicatorConfiguration c_config = config;
            _pushFilter = config.pushFilter;
            if (_pushFilter) {
                c_config.pushFilter = [](void *context, CBLDocument* doc, bool isDeleted) -> bool {
                    return ((Replicator*)context)->_pushFilter(Document(doc), isDeleted);
                };
            }
            _pullFilter = config.pullFilter;
            if (_pullFilter) {
                c_config.pullFilter = [](void *context, CBLDocument* doc, bool isDeleted) -> bool {
                    return ((Replicator*)context)->_pullFilter(Document(doc), isDeleted);
                };
            }
            c_config.context = this;
            _ref = (CBLRefCounted*) CBLReplicator_New(&c_config, &error);
            check(_ref, error);
        }

        void start(bool resetCheckpoint =false) {CBLReplicator_Start(ref(), resetCheckpoint);}
        void stop()                         {CBLReplicator_Stop(ref());}

        void setHostReachable(bool r)       {CBLReplicator_SetHostReachable(ref(), r);}
        void setSuspended(bool s)           {CBLReplicator_SetSuspended(ref(), s);}

        CBLReplicatorStatus status() const  {return CBLReplicator_Status(ref());}

        fleece::Dict pendingDocumentIDs() const {
            CBLError error;
            fleece::Dict result = CBLReplicator_PendingDocumentIDs(ref(), &error);
            check(result != nullptr, error);
            FLDict_Release(result);  // remove the extra ref the C function returned with
            return result;
        }

        bool isDocumentPending(fleece::slice docID) const {
            CBLError error;
            bool pending = CBLReplicator_IsDocumentPending(ref(), docID, &error);
            check(pending || error.code == 0, error);
            return pending;
        }

        using ChangeListener = cbl::ListenerToken<Replicator, const CBLReplicatorStatus&>;
        using DocumentListener = cbl::ListenerToken<Replicator, bool,
                                                    const std::vector<CBLReplicatedDocument>>;

        [[nodiscard]] ChangeListener addChangeListener(ChangeListener::Callback f) {
            auto l = ChangeListener(f);
            l.setToken( CBLReplicator_AddChangeListener(ref(), &_callChangeListener, l.context()) );
            return l;
        }

        [[nodiscard]] DocumentListener addDocumentListener(DocumentListener::Callback f) {
            auto l = DocumentListener(f);
            l.setToken( CBLReplicator_AddDocumentReplicationListener(ref(), &_callDocListener, l.context()) );
            return l;
        }

    private:
        static void _callChangeListener(void* _cbl_nullable context,
                                        CBLReplicator *repl,
                                        const CBLReplicatorStatus *status)
        {
            ChangeListener::call(context, Replicator(repl), *status);
        }

        static void _callDocListener(void* _cbl_nullable context,
                                     CBLReplicator *repl,
                                     bool isPush,
                                     unsigned numDocuments,
                                     const CBLReplicatedDocument* documents)
        {
            std::vector<CBLReplicatedDocument> docs(&documents[0], &documents[numDocuments]);
            DocumentListener::call(context, Replicator(repl), isPush, docs);
        }

        ReplicationFilter _pushFilter;
        ReplicationFilter _pullFilter;

        CBL_REFCOUNTED_BOILERPLATE(Replicator, RefCounted, CBLReplicator)
    };
}

CBL_ASSUME_NONNULL_END
