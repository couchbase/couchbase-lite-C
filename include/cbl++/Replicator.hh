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
#include <unordered_map>

// VOLATILE API: Couchbase Lite C++ API is not finalized, and may change in
// future releases.

CBL_ASSUME_NONNULL_BEGIN

namespace cbl {

    class Endpoint {
    public:
        void setURL(slice url) {
            CBLError error;
            _ref = std::shared_ptr<CBLEndpoint>(CBLEndpoint_CreateWithURL(url, &error), [](auto ref) {
                CBLEndpoint_Free(ref);
            });
            if (!_ref)
                throw error;
        }
#ifdef COUCHBASE_ENTERPRISE
        void setLocalDB(Database db) {
            _ref = std::shared_ptr<CBLEndpoint>(CBLEndpoint_CreateWithLocalDB(db.ref()), [](auto ref) {
                CBLEndpoint_Free(ref);
            });
        }
#endif
        CBLEndpoint* _cbl_nullable ref() const {return _ref.get();}
    private:
        std::shared_ptr<CBLEndpoint> _ref;
    };


    class Authenticator {
    public:
        void setBasic(slice username, slice password) {
            _ref = std::shared_ptr<CBLAuthenticator>(CBLAuth_CreatePassword(username, password),
                                                    [](auto ref) {
                CBLAuth_Free(ref);
            });
        }

        void setSession(slice sessionId, slice cookieName) {
            _ref = std::shared_ptr<CBLAuthenticator>(CBLAuth_CreateSession(sessionId, cookieName),
                                                     [](auto ref) {
                CBLAuth_Free(ref);
            });
        }
        CBLAuthenticator* _cbl_nullable ref() const {return _ref.get();}
    private:
        std::shared_ptr<CBLAuthenticator> _ref;
    };


    using ReplicationFilter = std::function<bool(Document, CBLDocumentFlags flags)>;

    using ConflictResolver = std::function<Document(slice docID,
                                                    const Document localDoc,
                                                    const Document remoteDoc)>;

    class ReplicationCollection {
    public:
        ReplicationCollection(Collection collection)
        :_collection(collection)
        { }
        
        Collection collection() const       {return _collection;}
        
        fleece::MutableArray channels       = fleece::MutableArray::newArray();
        fleece::MutableArray documentIDs    = fleece::MutableArray::newArray();

        ReplicationFilter pushFilter;
        ReplicationFilter pullFilter;
        
        ConflictResolver conflictResolver;
        
    private:
        Collection _collection;
    };

    class ReplicatorConfiguration {
    public:
        ReplicatorConfiguration(Database db, Endpoint endpoint)
        :_database(db)
        ,_endpoint(endpoint)
        { }
        
        ReplicatorConfiguration(std::vector<ReplicationCollection>collections, Endpoint endpoint)
        :_collections(collections)
        ,_endpoint(endpoint)
        { }
        
        Database database() const           {return _database;}
        Endpoint endpoint() const           {return _endpoint;}
        std::vector<ReplicationCollection> collections() const  {return _collections;}
        
        CBLReplicatorType replicatorType    = kCBLReplicatorTypePushAndPull;
        bool continuous                     = false;
        
        bool enableAutoPurge                = true;
        
        unsigned maxAttempts                = 0;
        unsigned maxAttemptWaitTime         = 0;
        
        unsigned heartbeat                  = 0;
        
        std::string networkInterface;

        Authenticator authenticator;
        CBLProxySettings* _cbl_nullable proxy = nullptr;
        fleece::MutableDict headers         = fleece::MutableDict::newDict();

        std::string pinnedServerCertificate;
        std::string trustedRootCertificates;

        fleece::MutableArray channels       = fleece::MutableArray::newArray();
        fleece::MutableArray documentIDs    = fleece::MutableArray::newArray();

        ReplicationFilter pushFilter;
        ReplicationFilter pullFilter;
        
        ConflictResolver conflictResolver;
        
    protected:
        friend class Replicator;
        
        /** Base config without database, collections, filters, and conflict resolver set. */
        operator CBLReplicatorConfiguration() const {
            CBLReplicatorConfiguration conf = {};
            conf.endpoint = _endpoint.ref();
            conf.replicatorType = replicatorType;
            conf.continuous = continuous;
            conf.disableAutoPurge = !enableAutoPurge;
            conf.maxAttempts = maxAttempts;
            conf.maxAttemptWaitTime = maxAttemptWaitTime;
            conf.heartbeat = heartbeat;
            conf.authenticator = authenticator.ref();
            conf.proxy = proxy;
            if (!headers.empty())
                conf.headers = headers;
            if (!networkInterface.empty())
                conf.networkInterface = slice(networkInterface);
            if (!pinnedServerCertificate.empty())
                conf.pinnedServerCertificate = slice(pinnedServerCertificate);
            if (!trustedRootCertificates.empty())
                conf.trustedRootCertificates = slice(trustedRootCertificates);
            return conf;
        }
        
    private:
        Database _database;
        Endpoint _endpoint;
        std::vector<ReplicationCollection> _collections;
    };

    class Replicator : private RefCounted {
    public:
        Replicator(const ReplicatorConfiguration& config)
        {
            // Get the current configured collections and populate one for the
            // default collection if the config is configured with the database:
            auto collections = config.collections();
            
            auto database = config.database();
            if (database) {
                assert(collections.empty());
                auto defaultCollection = database.getDefaultCollection();
                if (!defaultCollection) {
                    throw std::invalid_argument("default collection not exist");
                }
                ReplicationCollection col = ReplicationCollection(defaultCollection);
                col.channels = config.channels;
                col.documentIDs = config.documentIDs;
                col.pushFilter = config.pushFilter;
                col.pullFilter = config.pullFilter;
                col.conflictResolver = config.conflictResolver;
                collections.push_back(col);
            }
            
            // Created a shared collection map. The pointer of the collection map will be
            // used as a context.
            _collectionMap = std::shared_ptr<CollectionToReplCollectionMap>(new CollectionToReplCollectionMap());
            
            // Get base C config:
            CBLReplicatorConfiguration c_config = config;
            
            // Construct C replication collections to set to the c_config:
            std::vector<CBLReplicationCollection> replCols;
            for (int i = 0; i < collections.size(); i++) {
                ReplicationCollection& col = collections[i];
                
                CBLReplicationCollection replCol {};
                replCol.collection = col.collection().ref();
                
                if (!col.channels.empty()) {
                    replCol.channels = col.channels;
                }

                if (!col.documentIDs.empty()) {
                    replCol.documentIDs = col.documentIDs;
                }

                if (col.pushFilter) {
                    replCol.pushFilter = [](void* context,
                                            CBLDocument* cDoc,
                                            CBLDocumentFlags flags) -> bool {
                        auto doc = Document(cDoc);
                        auto map = (CollectionToReplCollectionMap*)context;
                        return map->find(doc.collection())->second.pushFilter(doc, flags);
                    };
                }
                
                if (col.pullFilter) {
                    replCol.pullFilter = [](void* context,
                                            CBLDocument* cDoc,
                                            CBLDocumentFlags flags) -> bool {
                        auto doc = Document(cDoc);
                        auto map = (CollectionToReplCollectionMap*)context;
                        return map->find(doc.collection())->second.pullFilter(doc, flags);
                    };
                }
                
                if (col.conflictResolver) {
                    replCol.conflictResolver = [](void* context,
                                                 FLString docID,
                                                 const CBLDocument* cLocalDoc,
                                                 const CBLDocument* cRemoteDoc) -> const CBLDocument*
                    {
                        auto localDoc = Document(cLocalDoc);
                        auto remoteDoc = Document(cRemoteDoc);
                        auto collection = localDoc ? localDoc.collection() : remoteDoc.collection();
                        
                        auto map = (CollectionToReplCollectionMap*)context;
                        auto resolved = map->find(collection)->second.
                            conflictResolver(slice(docID), localDoc, remoteDoc);
                        
                        auto ref = resolved.ref();
                        if (ref && ref != cLocalDoc && ref != cRemoteDoc) {
                            CBLDocument_Retain(ref);
                        }
                        return ref;
                    };
                }
                replCols.push_back(replCol);
                _collectionMap->insert({col.collection(), col});
            }
            
            c_config.collections = replCols.data();
            c_config.collectionCount = replCols.size();
            c_config.context = _collectionMap.get();
            
            CBLError error {};
            _ref = (CBLRefCounted*) CBLReplicator_Create(&c_config, &error);
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
        using DocumentReplicationListener = cbl::ListenerToken<Replicator, bool,
            const std::vector<CBLReplicatedDocument>>;

        [[nodiscard]] ChangeListener addChangeListener(ChangeListener::Callback f) {
            auto l = ChangeListener(f);
            l.setToken( CBLReplicator_AddChangeListener(ref(), &_callChangeListener, l.context()) );
            return l;
        }

        [[nodiscard]] DocumentReplicationListener addDocumentReplicationListener(DocumentReplicationListener::Callback f) {
            auto l = DocumentReplicationListener(f);
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
            DocumentReplicationListener::call(context, Replicator(repl), isPush, docs);
        }
        
        using CollectionToReplCollectionMap = std::unordered_map<Collection, ReplicationCollection>;
        std::shared_ptr<CollectionToReplCollectionMap> _collectionMap;
        
        CBL_REFCOUNTED_WITHOUT_COPY_MOVE_BOILERPLATE(Replicator, RefCounted, CBLReplicator)
        
    public:
        Replicator(const Replicator &other) noexcept
        :RefCounted(other)
        ,_collectionMap(other._collectionMap)
        { }
        
        Replicator(Replicator &&other) noexcept
        :RefCounted((RefCounted&&)other)
        ,_collectionMap(std::move(other._collectionMap))
        { }
        
        Replicator& operator=(const Replicator &other) noexcept {
            RefCounted::operator=(other);
            _collectionMap = other._collectionMap;
            return *this;
        }
        
        Replicator& operator=(Replicator &&other) noexcept {
            RefCounted::operator=((RefCounted&&)other);
            _collectionMap = std::move(other._collectionMap);
            return *this;
        }
        
        void clear() {
            RefCounted::clear();
            _collectionMap.reset();
        }
    };
}

CBL_ASSUME_NONNULL_END
