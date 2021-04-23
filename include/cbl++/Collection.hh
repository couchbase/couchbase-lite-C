//
// Collection.hh
//
// Copyright (c) 2021 Couchbase, Inc All rights reserved.
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
#include "cbl++/Database.hh"
#include "cbl/CBLCollection.h"
#include "fleece/Mutable.hh"
#include <functional>
#include <string>
#include <vector>

// PLEASE NOTE: This C++ wrapper API is provided as a convenience only.
// It is not considered part of the official Couchbase Lite API.

CBL_ASSUME_NONNULL_BEGIN

namespace cbl {
    class Document;
    class MutableDocument;


    using SaveConflictHandler = std::function<bool(MutableDocument documentBeingSaved,
                                                   Document conflictingDocument)>;


    class Collection : private RefCounted {
    public:

        std::string name() const                   {return asString(CBLCollection_Name(ref()));}

        Database database() const                  {return Database(CBLCollection_Database(ref()));}

        uint64_t count() const                     {return CBLCollection_Count(ref());}

        // Documents:

        inline Document getDocument(slice id) const;
        inline MutableDocument getMutableDocument(slice id) const;

        inline void saveDocument(MutableDocument &doc);

        _cbl_warn_unused
        inline bool saveDocument(MutableDocument &doc, CBLConcurrencyControl c);

        _cbl_warn_unused
        inline bool saveDocument(MutableDocument &doc, SaveConflictHandler conflictHandler);

        inline void deleteDocument(Document &doc);

        _cbl_warn_unused
        inline bool deleteDocument(Document &doc, CBLConcurrencyControl c);

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

        inline void purgeDocument(Document &doc);

        bool purgeDocumentByID(slice docID) {
            CBLError error;
            bool purged = CBLCollection_PurgeDocumentByID(ref(), docID, &error);
            if (!purged && error.code != 0)
                throw error;
            return purged;
        }

        // Indexes:

        void createValueIndex(slice name, CBLValueIndex index) {
            CBLError error;
            check(CBLCollection_CreateValueIndex(ref(), name, index, &error), error);
        }

        void createFullTextIndex(slice name, CBLFullTextIndex index) {
            CBLError error;
            check(CBLCollection_CreateFullTextIndex(ref(), name, index, &error), error);
        }

        void deleteIndex(slice name) {
            CBLError error;
            check(CBLCollection_DeleteIndex(ref(), name, &error), error);
        }

        fleece::MutableArray indexNames() {
            FLMutableArray flNames = CBLCollection_IndexNames(ref());
            fleece::MutableArray names(flNames);
            FLMutableArray_Release(flNames);
            return names;
        }

        // Listeners:

        using Listener = cbl::ListenerToken<Collection,const std::vector<slice>&>;

        [[nodiscard]] Listener addListener(Listener::Callback f) {
            auto l = Listener(f);
            l.setToken( CBLCollection_AddChangeListener(ref(), &_callListener, l.context()) );
            return l;
        }


        using DocumentListener = cbl::ListenerToken<Database,slice>;

        [[nodiscard]] DocumentListener addDocumentListener(slice docID,
                                                           DocumentListener::Callback f)
        {
            auto l = DocumentListener(f);
            l.setToken( CBLCollection_AddDocumentChangeListener(ref(), docID, &_callDocListener, l.context()) );
            return l;
        }


    private:
        static void _callListener(void* _cbl_nullable context,
                                  const CBLCollection *db,
                                  unsigned nDocs, FLString *docIDs)
        {
            std::vector<slice> vec((slice*)&docIDs[0], (slice*)&docIDs[nDocs]);
            Listener::call(context, Collection((CBLCollection*)db), vec);
        }

        static void _callDocListener(void* _cbl_nullable context,
                                     const CBLDatabase *db, FLString docID) {
            DocumentListener::call(context, Database((CBLDatabase*)db), docID);
        }


    private:
        CBL_REFCOUNTED_BOILERPLATE(Collection, RefCounted, CBLCollection)
    };

}

CBL_ASSUME_NONNULL_END
