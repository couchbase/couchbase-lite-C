//
// CBLDocument.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "CBLDocument.h"
#include "Internal.hh"
#include "c4Document+Fleece.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"

using namespace std;
using namespace fleece;


static inline string docIDOrGenerated(const char *docID) {
    char docIDBuf[32];
    if (!docID)
        docID = c4doc_generateID(docIDBuf, sizeof(docIDBuf));
    return string(docID);
}


struct CBLDocument : public fleece::RefCounted {

    // Core constructor
    CBLDocument(const string &docID,
                CBLDatabase *db,
                C4Document *d,
                bool isMutable)
    :_docID(docID)
    ,_db(db)
    ,_c4doc(d)
    ,_flDoc(_c4doc ? c4doc_createFleeceDoc(_c4doc) : nullptr)
    ,_properties(_flDoc.root().asDict())
    ,_mutable(isMutable)
    {
        if (_mutable)
            _properties = _properties ? FLDict_MutableCopy(_properties) : FLMutableDict_New();
    }

    // Construct a new document (not in any database yet)
    CBLDocument(const char *docID)
    :CBLDocument(docIDOrGenerated(docID), nullptr, nullptr, false)
    { }

    // Mutable copy of another CBLDocument
    CBLDocument(const CBLDocument* otherDoc)
    :CBLDocument(otherDoc->_docID,
                 otherDoc->_db,
                 c4doc_retain(otherDoc->_c4doc),
                 true)
    { }

    ~CBLDocument() {
        if (_mutable)
            FLValue_Release(_properties);
        c4doc_release(_c4doc);
    }

    const char* docID() const                   {return _docID.c_str();}
    uint64_t sequence() const                   {return _c4doc ? _c4doc->sequence : 0;}
    bool isMutable() const                      {return _mutable;}
    Dict properties() const                     {return _properties;}
    MutableDict mutableProperties()             {return _properties.asMutable();}

    CBLDocument* save(CBLDatabase* db _cblnonnull,
                      CBLConcurrencyControl concurrency,
                      CBLError* outError)
    {
        assert(_mutable);
        if (_db && _db != db) {
            if (outError)
                *(C4Error*)outError = c4error_make(LiteCoreDomain, kC4ErrorInvalidParameter,
                                                   "Saving doc to wrong database"_sl);
            return nullptr;
        }

        // Encode properties:
        alloc_slice body;
        {
            Encoder enc(c4db_getSharedFleeceEncoder(db->c4db));
            enc.writeValue(_properties);
            body = enc.finish();
            enc.detach();
        }

        // Save:
        C4Document *newDoc = nullptr;
        if (_c4doc) {
            newDoc = c4doc_update(_c4doc, body, 0, internal(outError));
        } else {
            C4DocPutRequest rq = {};
            rq.allocedBody = {body.buf, body.size};
            rq.docID = slice(_docID);
            rq.save = true;
            newDoc = c4doc_put(db->c4db, &rq, nullptr, internal(outError));
        }

        // Return a new CBLDocument:
        return newDoc ? new CBLDocument(_docID, db, newDoc, false) : nullptr;
    }

    bool deleteDoc(CBLConcurrencyControl concurrency,
                   CBLError* outError)
    {
        if (!_c4doc)
            return true;
        C4Document *newDoc = c4doc_update(_c4doc, nullslice, kRevDeleted, internal(outError));
        if (!newDoc)
            return false;
        c4doc_release(newDoc);
        return true;
    }

private:
    string _docID;
    Retained<CBLDatabase> const _db;
    C4Document* const _c4doc {nullptr};
    Doc _flDoc;
    Dict _properties {nullptr};
    bool _mutable {false};
};


const CBLDocument* cbl_db_getDocument(CBLDatabase* db, const char* docID) {
    C4Document* c4doc = c4doc_get(internal(db), slice(docID), true, nullptr);
    if (!c4doc)
        return nullptr;
    return retain(new CBLDocument(docID, db, c4doc, false));
}


CBLDocument* cbl_doc_new(const char *docID) {
    return retain(new CBLDocument(docID));
}


CBLDocument* cbl_doc_mutableCopy(const CBLDocument* doc) {
    return retain(new CBLDocument(doc));
}


const char* cbl_doc_id(const CBLDocument* doc)      {return doc->docID();}
uint64_t cbl_doc_sequence(const CBLDocument* doc)   {return doc->sequence();}
FLDict cbl_doc_properties(const CBLDocument* doc)   {return doc->properties();}
FLMutableDict cbl_doc_mutableProperties(CBLDocument* doc) {return doc->mutableProperties();}


const CBLDocument* cbl_db_saveDocument(CBLDatabase* db,
                                       CBLDocument* doc,
                                       CBLConcurrencyControl concurrency,
                                       CBLError* outError)
{
    return doc->save(db, concurrency, outError);
}
