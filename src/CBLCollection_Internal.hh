//
//  CBLCollection_Internal.hh
//
// Copyright (c) 2022 Couchbase, Inc All rights reserved.
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
#include "CBLCollection.h"
#include "CBLDatabase_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "CBLPrivate.h"
#include "CBLScope_Internal.hh"
#include "CBLVectorIndexConfig.hh"
#include "Defer.hh"

CBL_ASSUME_NONNULL_BEGIN

using CollectionSpec = C4Database::CollectionSpec;

using namespace litecore;

struct CBLCollection final : public CBLRefCounted {
    
public:
    
#pragma mark - CONSTRUCTORS:
    
    CBLCollection(C4Collection* c4col, CBLScope* scope, CBLDatabase* database)
    :_c4col(c4col, database)
    ,_scope(scope)
    ,_database(retain(database))
    ,_name(c4col->getName())
    {
        _fullName = alloc_slice(_scope->name());
        _fullName.append(".");
        _fullName.append(_name);
    }
    
    ~CBLCollection() {
        LOCK(_adoptMutex);
        if (!_adopted) {
            release(_database);
        }
    }
    
#pragma mark - ACCESSORS:
    
    Retained<CBLScope> scope() const noexcept   {return _scope;}
    slice name() const noexcept                 {return _name;}
    slice fullName() const noexcept             {return _fullName;}
    C4CollectionSpec spec() const noexcept      {return {_name, _scope->name()};}
    bool isValid() const noexcept               {return _c4col.isValid();}
    uint64_t count() const                      {return _c4col.useLocked()->getDocumentCount();}
    uint64_t lastSequence() const               {return static_cast<uint64_t>(_c4col.useLocked()->getLastSequence());}
    CBLDatabase* database() const               {return _database; }
    
#pragma mark - DOCUMENTS:
    
    RetainedConst<CBLDocument> getDocument(slice docID, bool allRevisions =false) const {
        return getDocument(docID, false, allRevisions);
    }

    Retained<CBLDocument> getMutableDocument(slice docID) {
        return getDocument(docID, true, true);
    }
    
    bool deleteDocument(const CBLDocument *doc, CBLConcurrencyControl concurrency) {
        CBLDocument::SaveOptions opt(concurrency);
        opt.deleting = true;
        return const_cast<CBLDocument*>(doc)->save(this, opt);
    }
    
    bool deleteDocument(slice docID) {
        auto c4col = _c4col.useLocked();
        C4Database::Transaction t(c4col->getDatabase());
        Retained<C4Document> c4doc = c4col->getDocument(docID, false, kDocGetCurrentRev);
        if (c4doc)
            c4doc = c4doc->update(fleece::nullslice, kRevDeleted);
        if (!c4doc)
            return false;
        t.commit();
        return true;
    }
    
    bool purgeDocument(slice docID) {
        return _c4col.useLocked()->purgeDocument(docID);
    }
    
    CBLTimestamp getDocumentExpiration(slice docID) {
        return static_cast<CBLTimestamp>(_c4col.useLocked()->getExpiration(docID));
    }

    void setDocumentExpiration(slice docID, CBLTimestamp expiration) {
        if (!_c4col.useLocked()->setExpiration(docID, C4Timestamp(expiration))) {
            C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Document not found");
        }
    }
    
#pragma mark - INDEXES:
    
    void createValueIndex(slice name, CBLValueIndexConfiguration config) {
        C4IndexOptions options = {};
        _c4col.useLocked()->createIndex(name, config.expressions,
                                        (C4QueryLanguage)config.expressionLanguage,
                                        kC4ValueIndex, &options);
    }
    
    void createFullTextIndex(slice name, CBLFullTextIndexConfiguration config) {
        C4IndexOptions options = {};
        options.ignoreDiacritics = config.ignoreAccents;
        
        std::string languageStr;
        if (config.language.buf) {
            languageStr = std::string(config.language);
            options.language = languageStr.c_str();
        }
        
        _c4col.useLocked()->createIndex(name, config.expressions,
                                        (C4QueryLanguage)config.expressionLanguage,
                                        kC4FullTextIndex, &options);
    }
    
    void createArrayIndex(slice name, CBLArrayIndexConfiguration config) {
        C4IndexOptions options = {};
        options.unnestPath = (char*)config.path.buf;
        
        auto exprs = config.expressions;
        if (!exprs.buf && config.expressionLanguage == kCBLJSONLanguage) {
            exprs = FLStr("[]");
        }
        
        config.expressions = exprs;
        _c4col.useLocked()->createIndex(name, exprs,
                                        (C4QueryLanguage)config.expressionLanguage,
                                        kC4ArrayIndex, &options);
    }
    
#ifdef COUCHBASE_ENTERPRISE
    
    void createVectorIndex(slice name, CBLVectorIndexConfiguration config) {
        if (!config.expression.buf) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "expression is required.");
        }
            
        if (config.dimensions < 2 || config.dimensions > 4096) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "dimensions must be >= 2 and <= 4096.");
        }
        
        if (config.centroids < 1 || config.centroids > 64000) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "centroids must be >= 1 and <= 64000.");
        }
        
        if (config.minTrainingSize > 0 && config.maxTrainingSize > 0 && config.minTrainingSize > config.maxTrainingSize) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "minTrainingSize must be <= maxTrainingSize.");
        }
        
        // Use default encoding if encoding is not specified:
        CBLVectorEncoding* enc = nullptr;
        if (!config.encoding) {
            enc = CBLVectorEncoding_CreateScalarQuantizer(kCBLSQ8);
            config.encoding = enc;
        }
        DEFER {
            CBLVectorEncoding_Free(enc);
        };
        
        C4VectorEncoding c4enc =  config.encoding->c4encoding();
        if (c4enc.type == kC4VectorEncodingPQ) {
            if (c4enc.pq_subquantizers < 2) {
                C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "Product Quantizer's subquantizers must be > 1.");
            } else if (config.dimensions % c4enc.pq_subquantizers != 0) {
                C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "Product Quantizer's subquantizers must be a factor of dimensions.");
            } else if (c4enc.bits < 4 || c4enc.bits > 12){
                C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "Product Quantizer's bits must be >= 4 and <= 12.");
            }
        }
        
        C4VectorIndexOptions vector {};
        vector.clustering = {};
        vector.clustering.type = kC4VectorClusteringFlat;
        vector.clustering.flat_centroids = config.centroids;
        vector.dimensions = config.dimensions;
        vector.lazy = config.isLazy;
        vector.metric = (C4VectorMetricType) config.metric;
        vector.encoding = c4enc;
        vector.minTrainingSize = config.minTrainingSize;
        vector.maxTrainingSize = config.maxTrainingSize;
        vector.numProbes = config.numProbes;
        
        C4IndexOptions options {};
        options.vector = vector;
        
        _c4col.useLocked()->createIndex(name, config.expression,
                                        (C4QueryLanguage)config.expressionLanguage,
                                        kC4VectorIndex, &options);
    }
    
    bool isIndexTrained(slice name) const {
        return _c4col.useLocked()->isIndexTrained(name);
    }

#endif
    
    void deleteIndex(slice name) {
        _c4col.useLocked()->deleteIndex(name);
    }

    fleece::MutableArray indexNames() {
        Doc doc(_c4col.useLocked()->getIndexesInfo());
        auto indexes = fleece::MutableArray::newArray();
        for (Array::iterator i(doc.root().asArray()); i; ++i) {
            Dict info = i.value().asDict();
            indexes.append(info["name"]);
        }
        return indexes;
    }
    
    Retained<CBLQueryIndex> getIndex(slice name);
    
    fleece::MutableArray indexesInfo() const {
        Doc doc(_c4col.useLocked()->getIndexesInfo());
        return doc.root().asArray().mutableCopy();
    }
    
#pragma mark - LISTENERS
    
    Retained<CBLListenerToken> addChangeListener(CBLCollectionChangeListener listener,
                                                 void* _cbl_nullable ctx)
    {
        return addListener([&]{ return _listeners.add(listener, ctx); });
    }
    
    Retained<CBLListenerToken> addDocumentListener(slice docID,
                                                   CBLCollectionDocumentChangeListener listener,
                                                   void* _cbl_nullable ctx);
        
#pragma mark - UTILS
    
    static alloc_slice collectionSpecToPath(C4CollectionSpec& spec) {
        alloc_slice ret(spec.scope.size + spec.name.size + 1);
        void* buf = const_cast<void*>(ret.buf);
        slice(spec.scope).copyTo(buf);
        ((uint8_t*)buf)[spec.scope.size] = '.';
        size_t nameOffset = spec.scope.size + 1;
        slice(spec.name).copyTo((uint8_t*)buf + nameOffset);
        return ret;
    }
    
protected:
    
    friend struct CBLDatabase;
    friend struct CBLDocument;
    friend struct cbl_internal::ListenerToken<CBLCollectionDocumentChangeListener>;
    
    // Called by the database to take an ownership.
    // Release the database to avoid the circular reference
    void adopt(CBLDatabase* db) {
        assert(_database == db);
        LOCK(_adoptMutex);
        if (!_adopted) {
            _scope->adopt(db);
            release(_database);
            _adopted = true;
        }
    }
    
    auto useLocked()                        { return _c4col.useLocked(); }
    template <class LAMBDA>
    void useLocked(LAMBDA callback)         { _c4col.useLocked(callback); }
    template <class RESULT, class LAMBDA>
    RESULT useLocked(LAMBDA callback)       { return _c4col.useLocked<RESULT>(callback); }
    
private:
    
    Retained<CBLDocument> getDocument(slice docID, bool isMutable, bool allRevisions) const {
        C4DocContentLevel content = (allRevisions ? kDocGetAll : kDocGetCurrentRev);
        Retained<C4Document> c4doc = nullptr;
        try {
            c4doc = _c4col.useLocked()->getDocument(docID, true, content);
        } catch (litecore::error& e) {
            if (e == litecore::error::BadDocID) {
                CBL_Log(kCBLLogDomainDatabase, kCBLLogWarning,
                        "Invalid document ID '%.*s' used", FMTSLICE(docID));
                return nullptr;
            }
            throw;
        }
        if (!c4doc || (!allRevisions && (c4doc->flags() & kDocDeleted)))
            return nullptr;
        return new CBLDocument(docID, const_cast<CBLCollection*>(this), c4doc, isMutable);
    }
    
#pragma mark - LISTENERS:
    
    Retained<CBLListenerToken> addListener(fleece::function_ref<Retained<CBLListenerToken>()> cb) {
        Retained<CBLListenerToken> token = cb();
        if (!_observer)
            _observer = _c4col.useLocked()->observe([this](C4CollectionObserver*) {
                this->collectionChanged();
            });
        return token;
    }
    
    void collectionChanged() {
        _database->notify(std::bind(&CBLCollection::callCollectionChangeListeners, this));
    }

    void callCollectionChangeListeners() {
        static const uint32_t kMaxChanges = 100;
        while (true) {
            C4CollectionObserver::Change c4changes[kMaxChanges];
            auto result = _observer->getChanges(c4changes, kMaxChanges);
            uint32_t nChanges = result.numChanges;
            if (nChanges == 0)
                break;

            if (!_listeners.empty()) {
                FLString docIDs[kMaxChanges];
                for (uint32_t i = 0; i < nChanges; ++i)
                docIDs[i] = c4changes[i].docID;
                
                CBLCollectionChange change = {};
                change.collection = this;
                change.numDocs = nChanges;
                change.docIDs = docIDs;
                _listeners.call(&change);
            }
        }
    }
    
#pragma mark - SHARED ACCESS LOCK :
    
    /** For safely accessing the c4collection and CBLDatabase pointer with the shared mutex with CBLDatabase's c4db access lock.
        @Note  Subclass for setting up the sentry for throwing NotOpen exception when the c4collection becomes invalid.
        @Note  Retain the shared_ptr of the CBLDatabase's c4db access lock to maintain the life time of the mutex
     */
    class C4CollectionAccessLock: public litecore::shared_access_lock<Retained<C4Collection>> {
    public:
        C4CollectionAccessLock(C4Collection* c4col, CBLDatabase* database)
        :shared_access_lock(std::move(c4col), *database->c4db())
        ,_c4db(database->c4db())
        ,_col(c4col)
        {
            _sentry = [this](C4Collection* c4col) {
                if (!_isValid()) {
                    C4Error::raise(LiteCoreDomain, kC4ErrorNotOpen, "Invalid collection: either deleted or db closed");
                }
            };
        }
        
        bool isValid() const noexcept {
            LOCK_GUARD lock(getMutex());
            return _isValid();
        }
        
    private:
        // Unsafe: need to call under lock:
        bool _isValid() const noexcept                  { return !_c4db->isClosedNoLock() && _col->isValid(); }
        
        CBLDatabase::SharedC4DatabaseAccessLock _c4db;  // For retaining the shared c4db lock
        C4Collection*                           _col;
    };
    
#pragma mark - VARIABLES :
    
    C4CollectionAccessLock                                  _c4col;     // Shared lock with _c4db
    
    alloc_slice                                             _name;
    alloc_slice                                             _fullName;
    Retained<CBLScope>                                      _scope;
    
    CBLDatabase*                                            _database;         // Retained unless being adopted
    bool                                                    _adopted {false};  // Adopted by the database
    mutable std::mutex                                      _adoptMutex;
    
    std::unique_ptr<C4CollectionObserver>                   _observer;
    Listeners<CBLCollectionChangeListener>                  _listeners;
    Listeners<CBLCollectionDocumentChangeListener>          _docListeners;
};

CBL_ASSUME_NONNULL_END
