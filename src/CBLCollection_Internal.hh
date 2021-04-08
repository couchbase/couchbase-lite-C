//
// CBLCollection_Internal.hh
//
// Copyright (C) 2020 Jens Alfke. All Rights Reserved.
//

#pragma once
#include "CBLCollection.h"
#include "CBLDatabase_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "Internal.hh"

#include "c4Collection.hh"

CBL_ASSUME_NONNULL_BEGIN


struct CBLCollection final : CBLRefCounted {
public:

    CBLDatabase* database()                         {return _database;}
    slice name()                                    {return _c4coll.useLocked().get()->getName();}
    uint64_t count() const                          {return _c4coll.useLocked().get()->getDocumentCount();}
    uint64_t lastSequence() const                   {return _c4coll.useLocked().get()->getLastSequence();}


#pragma mark - Documents:


    RetainedConst<CBLDocument> getDocument(slice docID, bool allRevisions =false) const {
        return _getDocument(docID, false, allRevisions);
    }

    Retained<CBLDocument> getMutableDocument(slice docID) {
        return _getDocument(docID, true, true);
    }

    bool deleteDocument(const CBLDocument *doc,
                        CBLConcurrencyControl concurrency)
    {
        CBLDocument::SaveOptions opt(concurrency);
        opt.deleting = true;
        return const_cast<CBLDocument*>(doc)->save(this, opt);
    }

    bool deleteDocument(slice docID) {
        auto c4coll = _c4coll.useLocked();
        C4Database::Transaction t(c4coll.get()->getDatabase());
        Retained<C4Document> c4doc = c4coll.get()->getDocument(docID, false, kDocGetCurrentRev);
        if (c4doc)
            c4doc = c4doc->update(nullslice, kRevDeleted);
        if (!c4doc)
            return false;
        t.commit();
        return true;
    }

    bool purgeDocument(slice docID) {
        return _c4coll.useLocked().get()->purgeDocument(docID);
    }

    CBLTimestamp getDocumentExpiration(slice docID) {
        return _c4coll.useLocked().get()->getExpiration(docID);
    }

    void setDocumentExpiration(slice docID, CBLTimestamp expiration) {
        _c4coll.useLocked().get()->setExpiration(docID, expiration);
    }

    bool moveDocument(CBLCollection *collection,
                                    slice docID,
                                    CBLCollection *toCollection,
                                    slice newDocID,
                                    CBLError* _cbl_nullable error);


#pragma mark - Indexes:


    void createValueIndex(slice name, CBLValueIndex index) {
        if (index.expressionLanguage == kCBLN1QLLanguage) {
            // CBL-1734: Support N1QL expressions
            C4Error::raise(LiteCoreDomain, kC4ErrorUnsupported, "N1QL expression is not supported yet.");
        }

        C4IndexOptions options = {};
        _c4coll.useLocked().get()->createIndex(name, index.expressions, kC4ValueIndex, &options);
    }

    void createFullTextIndex(slice name, CBLFullTextIndex index) {
        if (index.expressionLanguage == kCBLN1QLLanguage) {
            // CBL-1734: Support N1QL expressions
            C4Error::raise(LiteCoreDomain, kC4ErrorUnsupported, "N1QL expression is not supported yet.");
        }

        C4IndexOptions options = {};
        options.ignoreDiacritics = index.ignoreAccents;

        std::string languageStr;
        if (index.language.buf) {
            languageStr = std::string(index.language);
            options.language = languageStr.c_str();
        }
        _c4coll.useLocked().get()->createIndex(name, index.expressions, kC4FullTextIndex, &options);
    }

    void deleteIndex(slice name) {
        _c4coll.useLocked().get()->deleteIndex(name);
    }

    fleece::MutableArray indexNames() {
        Doc doc(_c4coll.useLocked().get()->getIndexesInfo());
        auto indexes = fleece::MutableArray::newArray();
        for (Array::iterator i(doc.root().asArray()); i; ++i) {
            Dict info = i.value().asDict();
            indexes.append(info["name"]);
        }
        return indexes;
    }


#pragma mark - Listeners:

#if 0
    Retained<CBLListenerToken> addListener(CBLCollectionChangeListener listener,
                                           void* _cbl_nullable ctx)
    {
        return addListener([&]{ return _listeners.add(listener, ctx); });
    }

    Retained<CBLListenerToken> addListener(CBLCollectionChangeDetailListener listener,
                                           void* _cbl_nullable ctx)
    {
        return addListener([&]{ return _detailListeners.add(listener, ctx); });
    }

    Retained<CBLListenerToken> addDocListener(slice docID,
                                              CBLDocumentChangeListener,
                                              void* _cbl_nullable context);
#endif

protected:
    friend CBLDocument;
    friend CBLDatabase;

    static CBLCollection* _cbl_nullable
    withC4Collection(C4Collection* _cbl_nullable c4coll,
                     CBLDatabase *database,
                     const litecore::access_lock<Retained<C4Database>> &owner)
    {
        if (!c4coll)
            return nullptr;
        auto &info = c4coll->extraInfo();
        if (info.pointer)
            return (CBLCollection*)info.pointer;
        // Instantiate a new CBLCollection and hook it up:
        Retained<CBLCollection> coll = new CBLCollection(c4coll, database, owner);
        info.pointer = coll.get();
        info.destructor = [](void *pointer) { release((CBLCollection*)pointer);};
        return std::move(coll).detach();
    }
    
    auto useLocked()                  { return _c4coll.useLocked(); }
    template <class LAMBDA>
    void useLocked(LAMBDA callback)   { _c4coll.useLocked(callback); }
    template <class RESULT, class LAMBDA>
    RESULT useLocked(LAMBDA callback) { return _c4coll.useLocked<RESULT>(callback); }

private:
    CBLCollection(C4Collection *c4coll,
                  CBLDatabase *database,
                  const litecore::access_lock<Retained<C4Database>> &owner)
    :_database(database)
    ,_c4coll(std::move(c4coll), owner)
    { }

    Retained<CBLDocument> _getDocument(slice docID, bool isMutable, bool allRevisions) const {
        C4DocContentLevel content = (allRevisions ? kDocGetAll : kDocGetCurrentRev);
        Retained<C4Document> c4doc = _c4coll.useLocked().get()->getDocument(docID, true, content);
        if (!c4doc || (!allRevisions && (c4doc->flags() & kDocDeleted)))
            return nullptr;
        return new CBLDocument(docID, const_cast<CBLCollection*>(this), c4doc, isMutable);
    }

#if 0
    Retained<CBLListenerToken> addListener(fleece::function_ref<Retained<CBLListenerToken>()> cb) {
        auto c4db = _c4coll.useLocked(); // locks DB mutex, so the callback can run thread-safe
        Retained<CBLListenerToken> token = cb();
        if (!_observer)
            _observer = c4db()->observe([this](C4DatabaseObserver*) { this->databaseChanged(); });
        return token;
    }
#endif


    // CBLDatabase retains C4Database.
    // C4Database retains C4Collection.
    // C4Collection retains CBLCollection (via its extraInfo).
    // Therefore CBLCollection does not retain CBLDatabase nor C4Collection.

    CBLDatabase* _database;
    litecore::shared_access_lock<C4Collection*> _c4coll;
};


CBL_ASSUME_NONNULL_END
