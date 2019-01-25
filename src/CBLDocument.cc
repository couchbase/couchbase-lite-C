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
#include "CBLDocument_Internal.hh"
#include "Util.hh"

using namespace std;
using namespace fleece;


bool CBLDocument::checkMutable(C4Error *outError) const {
    if (_usuallyTrue(_mutable))
        return true;
    setError(outError, LiteCoreDomain, kC4ErrorNotWriteable, "Document object is immutable"_sl);
    return false;
}


string CBLDocument::ensureDocID(const char *docID) {
    char docIDBuf[32];
    if (!docID)
        docID = c4doc_generateID(docIDBuf, sizeof(docIDBuf));
    return string(docID);
}


RetainedConst<CBLDocument> CBLDocument::save(CBLDatabase* db _cbl_nonnull,
                                             bool deleting,
                                             CBLConcurrencyControl concurrency,
                                             C4Error* outError) const
{
    if (!checkMutable(outError))
        return nullptr;
    if (_db && _db != db) {
        setError(outError, LiteCoreDomain, kC4ErrorInvalidParameter,
                 "Saving doc to wrong database"_sl);
        return nullptr;
    } else if (!_mutable) {
        return this;
    }

    c4::Transaction t(internal(db));
    if (!t.begin(outError))
        return nullptr;

    // Encode properties:
    alloc_slice body;
    if (!deleting) {
        Encoder enc(c4db_getSharedFleeceEncoder(internal(db)));
        enc.writeValue(properties());
        body = enc.finish();
        enc.detach();
    }

    // Save:
    c4::ref<C4Document> savingDoc = c4doc_retain(_c4doc);
    c4::ref<C4Document> newDoc = nullptr;
    C4Error c4err;

    bool retrying = false;
    do {
        C4RevisionFlags flags = (deleting ? kRevDeleted : 0);
        if (savingDoc) {
            newDoc = c4doc_update(savingDoc, body, flags, &c4err);
        } else {
            C4DocPutRequest rq = {};
            rq.allocedBody = {body.buf, body.size};
            rq.docID = slice(_docID);
            rq.revFlags = flags;
            rq.save = true;
            newDoc = c4doc_put(internal(db), &rq, nullptr, &c4err);
        }

        if (!newDoc && c4err == C4Error{LiteCoreDomain, kC4ErrorConflict}
                    && concurrency == kCBLConcurrencyControlLastWriteWins) {
            // Conflict; in last-write-wins mode, load current revision and retry:
            if (retrying)
                break;  // (but only once)
            savingDoc = c4doc_get(internal(db), slice(_docID), true, &c4err);
            if (savingDoc || c4err == C4Error{LiteCoreDomain, kC4ErrorNotFound})
                retrying = true;
        }
    } while (retrying);

    if (newDoc && t.commit(&c4err)) {
        // Success!
        return new CBLDocument(_docID, db, c4doc_retain(newDoc), false);
    } else {
        // Failure:
        if (outError)
            *outError = c4err;
        return nullptr;
    }
}


bool CBLDocument::deleteDoc(CBLConcurrencyControl concurrency, C4Error* outError) const {
    if (!_db) {
        setError(outError, LiteCoreDomain, kC4ErrorNotFound, "Document is not in any database"_sl);
        return false;
    }
    RetainedConst<CBLDocument> deleted = save(_db, true, concurrency, outError);
    return (deleted != nullptr);
}


bool CBLDocument::deleteDoc(CBLDatabase* db _cbl_nonnull,
                            const char* docID _cbl_nonnull,
                            C4Error* outError)
{
    c4::Transaction t(internal(db));
    if (!t.begin(outError))
        return false;
    c4::ref<C4Document> c4doc = c4doc_get(internal(db), slice(docID), true, outError);
    if (c4doc)
        c4doc = c4doc_update(c4doc, nullslice, kRevDeleted, outError);
    return c4doc && t.commit(outError);
}


void CBLDocument::initProperties() {
    if (_c4doc && _c4doc->selectedRev.body.buf)
        _properties = Value::fromData(_c4doc->selectedRev.body);
    if (_mutable) {
        if (_properties)
            _properties = _properties.asDict().mutableCopy();
        if (!_properties)
            _properties = MutableDict::newDict();
    } else {
        if (!_properties)
            _properties = Dict::emptyDict();
    }
}


Dict CBLDocument::properties() const {
    if (!_properties)
        const_cast<CBLDocument*>(this)->initProperties();
    return _properties.asDict();
}


char* CBLDocument::propertiesAsJSON() const {
    alloc_slice json;
    if (!_mutable && _c4doc)
        json = c4doc_bodyAsJSON(_c4doc, false, nullptr);        // fast path
    else
        json = allocCString(properties().toJSON());
    return allocCString(json);
}


bool CBLDocument::setPropertiesAsJSON(const char *json, C4Error* outError) {
    if (!checkMutable(outError))
        return false;
    Doc doc = Doc::fromJSON(slice(json));
    if (!doc) {
        setError(outError, FleeceDomain, kFLJSONError, "Invalid JSON"_sl);
        return false;
    }
    Dict root = doc.root().asDict();
    if (!root) {
        setError(outError, FleeceDomain, kFLJSONError, "properties must be a JSON dictionary"_sl);
        return false;
    }
    _properties = root.mutableCopy(kFLDeepCopyImmutables);
    return true;
}


#pragma mark - PUBLIC API:


static CBLDocument* getDocument(CBLDatabase* db, const char* docID, bool isMutable) CBLAPI {
    auto doc = retained(new CBLDocument(db, docID, isMutable));
    return doc->exists() ? retain(doc.get()) : nullptr;
}

const CBLDocument* cbl_db_getDocument(const CBLDatabase* db, const char* docID) CBLAPI {
    return getDocument((CBLDatabase*)db, docID, false);
}

CBLDocument* cbl_db_getMutableDocument(CBLDatabase* db, const char* docID) CBLAPI {
    return getDocument(db, docID, true);
}

CBLDocument* cbl_doc_new(const char *docID) CBLAPI {
    return retain(new CBLDocument(docID, true));
}

CBLDocument* cbl_doc_mutableCopy(const CBLDocument* doc) CBLAPI {
    return retain(new CBLDocument(doc));
}

const char* cbl_doc_id(const CBLDocument* doc) CBLAPI              {return doc->docID();}
uint64_t cbl_doc_sequence(const CBLDocument* doc) CBLAPI           {return doc->sequence();}
FLDict cbl_doc_properties(const CBLDocument* doc) CBLAPI           {return doc->properties();}
FLMutableDict cbl_doc_mutableProperties(CBLDocument* doc) CBLAPI   {return doc->mutableProperties();}

char* cbl_doc_propertiesAsJSON(const CBLDocument* doc) CBLAPI      {return doc->propertiesAsJSON();}

bool cbl_doc_setPropertiesAsJSON(CBLDocument* doc, const char *json, CBLError* outError) CBLAPI {
    return doc->setPropertiesAsJSON(json, internal(outError));
}

const CBLDocument* cbl_db_saveDocument(CBLDatabase* db,
                                       CBLDocument* doc,
                                       CBLConcurrencyControl concurrency,
                                       CBLError* outError) CBLAPI
{
    return retain(doc->save(db, false, concurrency, internal(outError)).get());
}

bool cbl_doc_delete(const CBLDocument* doc _cbl_nonnull,
                    CBLConcurrencyControl concurrency,
                    CBLError* outError) CBLAPI
{
    return doc->deleteDoc(concurrency, internal(outError));
}

bool cbl_db_deleteDocument(CBLDatabase* db _cbl_nonnull,
                           const char* docID _cbl_nonnull,
                           CBLError* outError) CBLAPI
{
    return CBLDocument::deleteDoc(db, docID, internal(outError));
}

bool cbl_doc_purge(const CBLDocument* doc _cbl_nonnull,
                   CBLError* outError) CBLAPI
{
    return cbl_db_purgeDocument(doc->database(), doc->docID(), outError);
}

bool cbl_db_purgeDocument(CBLDatabase* db _cbl_nonnull,
                          const char* docID _cbl_nonnull,
                          CBLError* outError) CBLAPI
{
    return c4db_purgeDoc(internal(db), slice(docID), internal(outError));
}
