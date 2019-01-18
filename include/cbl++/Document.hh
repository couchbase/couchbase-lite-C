//
// Document.hh
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
#include "Database.hh"
#include "CBLDocument.h"
#include "fleece/Mutable.hh"

namespace cbl {
    class MutableDocument;

    class Document : protected RefCounted {
    public:
        inline MutableDocument mutableCopy() const;

        bool deleteDoc(CBLConcurrencyControl concurrency =kCBLConcurrencyControlFailOnConflict) const {
            CBLError error;
            bool deleted = cbl_doc_delete(ref(), concurrency, &error);
            if (!deleted && error.code != 0)
                throw error;
            return deleted;
        }

        bool purge() const {
            CBLError error;
            bool purged = cbl_doc_purge(ref(), &error);
            if (!purged && error.code != 0)
                throw error;
            return purged;
        }

        const char* _cblnonnull id() const              {return cbl_doc_id(ref());}

        uint64_t sequence() const                       {return cbl_doc_sequence(ref());}

        fleece::Dict properties() const                 {return cbl_doc_properties(ref());}

        char* _cblnonnull propertiesAsJSON(const CBLDocument* _cblnonnull);

        bool cbl_doc_setPropertiesAsJSON(CBLDocument* _cblnonnull,
                                         const char *json _cblnonnull,
                                         CBLError*);
        fleece::Value operator[] (const char *key _cblnonnull) const {return properties()[key];}


    protected:
        Document(CBLRefCounted* r)                      :RefCounted(r) { }
        friend class Database;

        CBL_REFCOUNTED_BOILERPLATE(Document, RefCounted, const CBLDocument)
    };


    class MutableDocument : public Document {
    public:
        explicit MutableDocument(const char *docID)     :Document(cbl_doc_new(docID)) { }

        fleece::MutableDict properties()                {return cbl_doc_mutableProperties(ref());}

        template <typename T>
        void set(const char *key _cblnonnull, T val)    {properties().set(fleece::slice(key), val);}

        fleece::keyref<fleece::MutableDict,fleece::slice> operator[] (const char *key)
                                                        {return properties()[fleece::slice(key)];}

        friend class Database;
        friend class Document;
        CBL_REFCOUNTED_BOILERPLATE(MutableDocument, Document, CBLDocument)
    };


    inline Document Database::getDocument(const char *id _cblnonnull) const {
        return Document(cbl_db_getDocument(ref(), id));
    }

    inline MutableDocument Database::getMutableDocument(const char *id _cblnonnull) const {
        return MutableDocument(cbl_db_getMutableDocument(ref(), id));
    }


    inline Document Database::saveDocument(MutableDocument &doc, CBLConcurrencyControl c) {
        CBLError error;
        auto saved = cbl_db_saveDocument(ref(), doc.ref(), c, &error);
        check(saved, error);
        return Document(saved);
    }


    inline MutableDocument Document::mutableCopy() const {
        return MutableDocument(cbl_doc_mutableCopy(ref()));
    }


}
