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
#include "CBLScope_Internal.hh"
#include "CBLPrivate.h"

CBL_ASSUME_NONNULL_BEGIN

struct CBLCollection final : public CBLRefCounted {
    
public:
    
#pragma mark - CONSTRUCTORS:
    
    CBLCollection(C4Collection* c4col, CBLDatabase* database)
    :_database(database)
    ,_c4col(c4col)
    {
        // Note: To avoid the case of not being able to get the scope object when
        // the collection is invalid. Get the scope right away.
        _scope = _database->getScope(_c4col->getScope());
    }
    
#pragma mark - ACCESSORS:
    
    CBLScope* scope() noexcept              {return _scope;}
    slice name() const noexcept             {return _c4col->getName();}
    bool isValid() const noexcept           {return _c4col->isValid();}
    uint64_t count() const                  {return _c4col->getDocumentCount();}
    
    // Return the database or throw if the database is released or the collection is invalid
    CBLDatabase* database() const {
        // Note: checking isValid() alone is not enough as c4db may not be necessary
        // to be releaed when the CBLDatabase is release.
        if (!_database || !isValid()) {
            C4Error::raise(LiteCoreDomain, kC4ErrorNotOpen,
                           "Invalid collection: either deleted, or db closed");
        }
        return _database;
    }
    
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
        auto c4db = database()->useLocked();
        C4Database::Transaction t(c4db);
        Retained<C4Document> c4doc = _c4col->getDocument(docID, false, kDocGetCurrentRev);
        if (c4doc)
            c4doc = c4doc->update(fleece::nullslice, kRevDeleted);
        if (!c4doc)
            return false;
        t.commit();
        return true;
    }
    
    bool purgeDocument(slice docID) {
        auto lock = database()->useLocked();
        return _c4col->purgeDocument(docID);
    }
    
    CBLTimestamp getDocumentExpiration(slice docID) {
        auto lock = database()->useLocked();
        return static_cast<CBLTimestamp>(_c4col->getExpiration(docID));
    }

    void setDocumentExpiration(slice docID, CBLTimestamp expiration) {
        auto lock = database()->useLocked();
        _c4col->setExpiration(docID, C4Timestamp(expiration));
    }
    
#pragma mark - INDEXES:
    
    void createValueIndex(slice name, CBLValueIndexConfiguration config) {
        C4IndexOptions options = {};
        auto lock = database()->useLocked();
        _c4col->createIndex(name, config.expressions,
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
        
        auto lock = database()->useLocked();
        _c4col->createIndex(name, config.expressions,
                            (C4QueryLanguage)config.expressionLanguage,
                            kC4FullTextIndex, &options);
    }

    void deleteIndex(slice name) {
        auto lock = database()->useLocked();
        _c4col->deleteIndex(name);
    }

    fleece::MutableArray indexNames() {
        auto lock = database()->useLocked();
        Doc doc(_c4col->getIndexesInfo());
        auto indexes = fleece::MutableArray::newArray();
        for (Array::iterator i(doc.root().asArray()); i; ++i) {
            Dict info = i.value().asDict();
            indexes.append(info["name"]);
        }
        return indexes;
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
        
protected:
    
    friend struct CBLDatabase;
    friend struct CBLDocument;
    friend class cbl_internal::AllConflictsResolver;
    friend struct cbl_internal::ListenerToken<CBLCollectionDocumentChangeListener>;
    
    Retained<C4Collection>& c4col()                 {return _c4col;}
    
    /** Called by the database to invalidate the _database pointer when the database is released. */
    void close() {
        auto lock = database()->useLocked();
        _database = nullptr;
    }
    
private:
    
    Retained<CBLDocument> getDocument(slice docID, bool isMutable, bool allRevisions) const {
        C4DocContentLevel content = (allRevisions ? kDocGetAll : kDocGetCurrentRev);
        Retained<C4Document> c4doc = nullptr;
        try {
            auto lock = database()->useLocked();
            c4doc = _c4col->getDocument(docID, true, content);
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
        auto lock = database()->useLocked();
        Retained<CBLListenerToken> token = cb();
        if (!_observer)
            _observer = _c4col->observe([this](C4CollectionObserver*) { this->collectionChanged(); });
        return token;
    }
    
    void collectionChanged() {
        try {
            database()->notify(std::bind(&CBLCollection::callCollectionChangeListeners, this));
        } catch (...) { }
    }

    void callCollectionChangeListeners() {
        static const uint32_t kMaxChanges = 100;
        while (true) {
            C4CollectionObserver::Change c4changes[kMaxChanges];
            bool external;
            uint32_t nChanges = _observer->getChanges(c4changes, kMaxChanges, &external);
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
    
    CBLDatabase* _cbl_nullable                          _database; // Not retain to prevent retain cycle
    Retained<C4Collection>                              _c4col;
    Retained<CBLScope>                                  _scope;
    
    std::unique_ptr<C4CollectionObserver>               _observer;
    Listeners<CBLCollectionChangeListener>              _listeners;
    Listeners<CBLCollectionDocumentChangeListener>      _docListeners;
};

CBL_ASSUME_NONNULL_END
