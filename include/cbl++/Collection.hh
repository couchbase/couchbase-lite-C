//
// Collection.hh
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
#include "cbl++/Base.hh"
#include "cbl/CBLCollection.h"
#include "cbl/CBLScope.h"
#include "fleece/Mutable.hh"
#include <functional>
#include <string>
#include <vector>

// VOLATILE API: Couchbase Lite C++ API is not finalized, and may change in
// future releases.

CBL_ASSUME_NONNULL_BEGIN

namespace cbl {
    class Document;
    class MutableDocument;
    class CollectionChange;
    class DocumentChange;

    using CollectionConflictHandler = std::function<bool(MutableDocument documentBeingSaved,
                                                         Document conflictingDocument)>;

    class Collection : private RefCounted {
    public:
        // Accessors:
        
        std::string name() const                    {return asString(CBLCollection_Name(ref()));}
        std::string scopeName() const               {return asString(CBLScope_Name(CBLCollection_Scope(ref())));}
        uint64_t count() const                      {return CBLCollection_Count(ref());}
        
        // Documents:
        
        inline Document getDocument(slice id) const;
        inline MutableDocument getMutableDocument(slice id) const;

        inline void saveDocument(MutableDocument &doc);

        _cbl_warn_unused
        inline bool saveDocument(MutableDocument &doc, CBLConcurrencyControl c);

        _cbl_warn_unused
        inline bool saveDocument(MutableDocument &doc, CollectionConflictHandler conflictHandler);

        inline void deleteDocument(Document &doc);

        _cbl_warn_unused
        inline bool deleteDocument(Document &doc, CBLConcurrencyControl c);

        inline void purgeDocument(Document &doc);
        
        bool purgeDocument(slice docID) {
            CBLError error;
            bool purged = CBLCollection_PurgeDocumentByID(ref(), docID, &error);
            if (!purged && error.code != 0)
                throw error;
            return purged;
        }
        
        time_t getDocumentExpiration(slice docID) const {
            CBLError error;
            time_t exp = CBLCollection_GetDocumentExpiration(ref(), docID, &error);
            check(exp >= 0, error);
            return exp;
        }

        void setDocumentExpiration(slice docID, time_t expiration) {
            CBLError error;
            check(CBLCollection_SetDocumentExpiration(ref(), docID, expiration, &error), error);
        }
        
        // Indexes:

        void createValueIndex(slice name, CBLValueIndexConfiguration config) {
            CBLError error;
            check(CBLCollection_CreateValueIndex(ref(), name, config, &error), error);
        }
        
        void createFullTextIndex(slice name, CBLFullTextIndexConfiguration config) {
            CBLError error;
            check(CBLCollection_CreateFullTextIndex(ref(), name, config, &error), error);
        }

        void deleteIndex(slice name) {
            CBLError error;
            check(CBLCollection_DeleteIndex(ref(), name, &error), error);
        }

        fleece::RetainedArray getIndexNames() {
            CBLError error;
            FLMutableArray flNames = CBLCollection_GetIndexNames(ref(), &error);
            check(flNames, error);
            fleece::RetainedArray names(flNames);
            FLArray_Release(flNames);
            return names;
        }
        
        // Listeners:
        
        using CollectionChangeListener = cbl::ListenerToken<CollectionChange*>;

        [[nodiscard]] CollectionChangeListener addChangeListener(CollectionChangeListener::Callback f) {
            auto l = CollectionChangeListener(f);
            l.setToken( CBLCollection_AddChangeListener(ref(), &_callListener, l.context()) );
            return l;
        }


        using CollectionDocumentChangeListener = cbl::ListenerToken<DocumentChange*>;

        [[nodiscard]] CollectionDocumentChangeListener addDocumentChangeListener(slice docID,
                                                                                 CollectionDocumentChangeListener::Callback f)
        {
            auto l = CollectionDocumentChangeListener(f);
            l.setToken( CBLCollection_AddDocumentChangeListener(ref(), docID, &_callDocListener, l.context()) );
            return l;
        }

        
    protected:
        
        static Collection adopt(const CBLCollection* _cbl_nullable d, CBLError *error) {
            if (!d && error->code != 0)
                throw *error;
            Collection col;
            col._ref = (CBLRefCounted*)d;
            return col;
        }
        
        friend class Database;
        
        CBL_REFCOUNTED_BOILERPLATE(Collection, RefCounted, CBLCollection);
    
    private:
        
        static void _callListener(void* _cbl_nullable context, const CBLCollectionChange* change) {
            Collection col = Collection((CBLCollection*)change->collection);
            std::vector<slice> docIDs((slice*)&change->docIDs[0], (slice*)&change->docIDs[change->numDocs]);
            auto ch = std::make_unique<CollectionChange>(col, docIDs);
            CollectionChangeListener::call(context, ch.get());
        }

        static void _callDocListener(void* _cbl_nullable context, const CBLDocumentChange* change) {
            Collection col = Collection((CBLCollection*)change->collection);
            slice docID = change->docID;
            auto ch = std::make_unique<DocumentChange>(col, docID);
            CollectionDocumentChangeListener::call(context, ch.get());
        }
    };

    class CollectionChange {
    public:
        
        Collection& collection()                    {return _collection;}
        std::vector<slice>& docIDs()                {return _docIDs;}
        
        CollectionChange(Collection collection, std::vector<slice> docIDs)
        :_collection(std::move(collection))
        ,_docIDs(std::move(docIDs))
        { }

    private:
        
        Collection _collection;
        std::vector<slice> _docIDs;
    };

    class DocumentChange {
    public:
        
        Collection& collection()                    {return _collection;}
        slice& docID()                              {return _docID;}

        DocumentChange(Collection collection, slice docID)
        :_collection(std::move(collection))
        ,_docID(std::move(docID))
        { }
        
    private:
        
        Collection _collection;
        slice _docID;
    };
}

CBL_ASSUME_NONNULL_END
