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
#include "CBLPrivate.h"
#include "CBLDocument_Internal.hh"
#include "CBLBlob_Internal.hh"
#include "c4Private.h"
#include "c4Transaction.hh"
#include "Util.hh"
#include <mutex>

using namespace std;
using namespace fleece;


static string ensureDocID(const char *docID) {
    char docIDBuf[32];
    if (!docID)
        docID = c4doc_generateID(docIDBuf, sizeof(docIDBuf));
    return string(docID);
}

static C4Document* getC4Doc(CBLDatabase *db, const string &docID, bool allRevisions) {
    return db->use<C4Document*>([&](C4Database *c4db) {
        C4Document *doc;
        if (allRevisions) {
            doc = c4doc_get(c4db, slice(docID), true, nullptr);
        } else {
            doc = c4doc_getSingleRevision(c4db, slice(docID), nullslice, true, nullptr);
            if (doc && doc->flags & kDocDeleted) {
                c4doc_release(doc);
                doc = nullptr;
            }
        }
        return doc;
    });
}


// Core constructor
CBLDocument::CBLDocument(const string &docID,
                         CBLDatabase *db,
                         C4Document *d,          // must be a +1 ref, or null
                         bool isMutable)
:_docID(docID)
,_db(db)
,_c4doc(d)
,_mutable(isMutable)
{
    if (_c4doc)
        _c4doc->extraInfo = {this, nullptr};
}


// Construct a new document (not in any database yet)
CBLDocument::CBLDocument(const char *docID, bool isMutable)
:CBLDocument(ensureDocID(docID), nullptr, nullptr, isMutable)
{ }


// Construct on an existing document
CBLDocument::CBLDocument(CBLDatabase *db, const string &docID, bool isMutable, bool allRevisions)
:CBLDocument(docID, db, getC4Doc(db, docID, allRevisions), isMutable)
{ }


// Mutable copy of another CBLDocument
CBLDocument::CBLDocument(const CBLDocument* otherDoc)
:CBLDocument(otherDoc->_docID,
             otherDoc->_db,
             c4doc_retain(otherDoc->_c4doc),
             true)
{
    if (otherDoc->isMutable()) {
        LOCK(otherDoc->_mutex);
        if (otherDoc->_properties)
            _properties = otherDoc->_properties.asDict().mutableCopy(kFLDeepCopyImmutables);
    }
}


// Document loaded from db without a C4Document (e.g. a replicator validation callback)
CBLDocument::CBLDocument(CBLDatabase *db,
            const string &docID,
            C4RevisionFlags revFlags,
            Dict body)
:CBLDocument(docID, db, nullptr, false)
{
    _properties = body;
}


CBLDocument::~CBLDocument() {
}


bool CBLDocument::checkMutable(C4Error *outError) const {
    if (_usuallyTrue(_mutable))
        return true;
    setError(outError, LiteCoreDomain, kC4ErrorNotWriteable, "Document object is immutable"_sl);
    return false;
}


#pragma mark - SAVING / DELETING:


RetainedConst<CBLDocument> CBLDocument::save(CBLDatabase* db _cbl_nonnull,
                                             const SaveOptions &opt,
                                             C4Error* outError)
{
    LOCK(_mutex);

    if (!opt.deleting && !checkMutable(outError))
        return nullptr;
    if (_db && _db != db) {
        setError(outError, LiteCoreDomain, kC4ErrorInvalidParameter,
                 "Saving doc to wrong database"_sl);
        return nullptr;
    }

    c4::ref<C4Document> newDoc = nullptr;
    db->use([&](C4Database *c4db) {
        c4::Transaction t(c4db);
        if (!t.begin(outError))
            return;

        // Encode properties:
        alloc_slice body;
        if (!opt.deleting) {
            body = encodeBody(db, c4db, outError);
            if (!body)
                return;
        }

        // Save:
        c4::ref<C4Document> savingDoc = c4doc_retain(_c4doc);
        C4Error c4err;

        bool retrying;
        do {
            retrying = false;
            C4RevisionFlags flags = (opt.deleting ? kRevDeleted : 0);
            if (savingDoc) {
                // Update existing doc:
                newDoc = c4doc_update(savingDoc, body, flags, &c4err);
            } else {
                // Create new doc:
                C4DocPutRequest rq = {};
                rq.allocedBody = {body.buf, body.size};
                rq.docID = slice(_docID);
                rq.revFlags = flags;
                rq.save = true;
                newDoc = c4doc_put(c4db, &rq, nullptr, &c4err);
            }

            if (!newDoc && c4err == C4Error{LiteCoreDomain, kC4ErrorConflict}) {
                // Conflict!
                if (opt.conflictHandler) {
                    // Custom conflict resolution:
                    Retained<CBLDocument> conflictingDoc = new CBLDocument(_db, _docID, false);
                    if (!conflictingDoc->exists())
                        conflictingDoc = nullptr;
                    if (!opt.conflictHandler(opt.context, this, conflictingDoc)) {
                        c4err = {LiteCoreDomain, kC4ErrorConflict};
                        break;
                    }
                    body = encodeBody(db, c4db, &c4err);
                    if (!body)
                        break;
                    savingDoc = conflictingDoc ? c4doc_retain(conflictingDoc->_c4doc) : nullptr;
                    retrying = true;
                } else if (opt.concurrency == kCBLConcurrencyControlLastWriteWins) {
                    // Last-write-wins; load current revision and retry:
                    savingDoc = c4doc_getSingleRevision(c4db, slice(_docID), nullslice, true,
                                                        &c4err);
                    if (!savingDoc && c4err != C4Error{LiteCoreDomain, kC4ErrorNotFound})
                        break;
                    retrying = true;
                }
            }
        } while (retrying);

        if (!newDoc || !t.commit(&c4err)) {
            // Failure:
            newDoc = nullptr;
            if (outError)
                *outError = c4err;
        }
    });

    if (!newDoc)
        return nullptr;
    return new CBLDocument(_docID, db, c4doc_retain(newDoc), false);
}


bool CBLDocument::deleteDoc(CBLConcurrencyControl concurrency, C4Error* outError) {
    if (!_db) {
        setError(outError, LiteCoreDomain, kC4ErrorNotFound, "Document is not in any database"_sl);
        return false;
    }
    SaveOptions opt(concurrency);
    opt.deleting = true;
    RetainedConst<CBLDocument> deleted = save(_db, opt, outError);
    return (deleted != nullptr);
}


/*static*/ bool CBLDocument::deleteDoc(CBLDatabase* db _cbl_nonnull,
                            const char* docID _cbl_nonnull,
                            C4Error* outError)
{
    return db->use<bool>([&](C4Database *c4db) {
        c4::Transaction t(c4db);
        if (!t.begin(outError))
            return false;
        c4::ref<C4Document> c4doc = c4doc_getSingleRevision(c4db, slice(docID), nullslice,
                                                            false, outError);
        if (c4doc)
            c4doc = c4doc_update(c4doc, nullslice, kRevDeleted, outError);
        return c4doc && t.commit(outError);
    });
}


#pragma mark - CONFLICT RESOLUTION:


bool CBLDocument::selectRevision(slice revID) {
    LOCK(_mutex);
    _properties = nullptr;
    return c4doc_selectRevision(_c4doc, revID, true, nullptr);
}


bool CBLDocument::selectNextConflictingRevision() {
    LOCK(_mutex);
    _properties = nullptr;
    while (c4doc_selectNextLeafRevision(_c4doc, true, true, nullptr))
        if (_c4doc->selectedRev.flags & kRevIsConflict)
            return true;
    return false;
}


bool CBLDocument::resolveConflict(Resolution resolution, const CBLDocument *mergeDoc, CBLError* outError)
{
    LOCK(_mutex);
    C4Error *c4err = internal(outError);
    _properties = nullptr;

    slice winner(_c4doc->selectedRev.revID), loser(_c4doc->revID);
    if (resolution != Resolution::useRemote)
        std::swap(winner, loser);

    return _db->use<bool>([&](C4Database *c4db) {
        c4::Transaction t(c4db);
        if (!t.begin(c4err))
            return false;

        alloc_slice mergeBody;
        C4RevisionFlags mergeFlags = 0;
        if (resolution == Resolution::useMerge) {
            if (mergeDoc) {
                mergeBody = mergeDoc->encodeBody(_db, c4db, c4err);
                if (!mergeBody)
                    return false;
            } else {
                mergeBody = alloc_slice(size_t(0));
                mergeFlags = kRevDeleted;
            }
        } else {
            assert(!mergeDoc);
        }

        return c4doc_resolveConflict(_c4doc, winner, loser, mergeBody, mergeFlags, c4err)
            && _db->use<bool>([&](C4Database*) { return c4doc_save(_c4doc, 0, c4err); })
            && t.commit(c4err);
    });
}


#pragma mark - PROPERTIES:


Dict CBLDocument::properties() const {
    LOCK(_mutex);
    if (!_properties) {
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
    return _properties.asDict();
}


char* CBLDocument::propertiesAsJSON() const {
    LOCK(_mutex);
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
    LOCK(_mutex);
    _properties = root.mutableCopy(kFLDeepCopyImmutables);
    return true;
}


alloc_slice CBLDocument::encodeBody(CBLDatabase *db _cbl_nonnull, C4Database *c4db _cbl_nonnull, C4Error *outError) const {
    LOCK(_mutex);
    // Save new blobs:
    if (!saveBlobs(db, outError))
        return nullslice;
    SharedEncoder enc(c4db_getSharedFleeceEncoder(c4db));
    enc.writeValue(properties());
    FLError flErr;
    alloc_slice body = enc.finish(&flErr);
    if (!body)
        c4error_return(FleeceDomain, flErr, nullslice, outError);
    return body;
}


#pragma mark - BLOBS:


CBLDocument::UnretainedValueToBlobMap* CBLDocument::sNewBlobs = nullptr;
mutex sNewBlobsMutex;


CBLBlob* CBLDocument::getBlob(FLDict dict) {
    LOCK(_mutex);
    // Is it already registered by a previous call to getBlob?
    auto i = _blobs.find(dict);
    if (i != _blobs.end())
        return i->second;
    // Is it a NewBlob?
    if (Dict(dict).asMutable()) {
        CBLNewBlob *newBlob = findNewBlob(dict);
        if (newBlob)
            return newBlob;
    }
    // Not found; create a new blob and remember it:
    auto blob = retained(new CBLBlob(this, dict));
    if (!blob->valid())
        return nullptr;
    _blobs.insert({dict, blob});
    return blob;
}


void CBLDocument::registerNewBlob(CBLNewBlob* blob) {
    LOCK(sNewBlobsMutex);
    if (!sNewBlobs)
        sNewBlobs = new UnretainedValueToBlobMap;
    sNewBlobs->insert({blob->properties(), blob});
}


void CBLDocument::unregisterNewBlob(CBLNewBlob* blob) {
    lock_guard<mutex> lock(sNewBlobsMutex);
    if (sNewBlobs)
        sNewBlobs->erase(blob->properties());
}


CBLNewBlob* CBLDocument::findNewBlob(FLDict dict) {
    LOCK(sNewBlobsMutex);
    auto i = sNewBlobs->find(dict);
    if (i == sNewBlobs->end())
        return nullptr;
    return i->second;
}


bool CBLDocument::saveBlobs(CBLDatabase *db, C4Error *outError) const {
    // Walk through the Fleece object tree, looking for mutable blob Dicts to install.
    // We can skip any immutable collections (they can't contain new blobs.)
    if (!isMutable())
        return true;
    LOCK(_mutex);
    for (DeepIterator i(properties()); i; ++i) {
        Dict dict = i.value().asDict();
        if (dict) {
            if (!dict.asMutable()) {
                i.skipChildren();
            } else if (CBL_IsBlob(dict)) {
                CBLNewBlob *blob = findNewBlob(dict);
                if (blob) {
                    if (!blob->install(db, outError))
                        return false;
                }
                i.skipChildren();
            }
        } else if (!i.value().asArray().asMutable()) {
            i.skipChildren();
        }
    }
    return true;
}


#pragma mark - PUBLIC API:


static CBLDocument* getDocument(CBLDatabase* db, const char* docID, bool isMutable) CBLAPI {
    auto doc = retained(new CBLDocument(db, docID, isMutable));
    return doc->exists() ? retain(doc.get()) : nullptr;
}

const CBLDocument* CBLDatabase_GetDocument(const CBLDatabase* db, const char* docID) CBLAPI {
    return getDocument((CBLDatabase*)db, docID, false);
}

CBLDocument* CBLDatabase_GetMutableDocument(CBLDatabase* db, const char* docID) CBLAPI {
    return getDocument(db, docID, true);
}

CBLDocument* CBLDocument_New(const char *docID) CBLAPI {
    return retain(new CBLDocument(docID, true));
}

CBLDocument* CBLDocument_MutableCopy(const CBLDocument* doc) CBLAPI {
    return retain(new CBLDocument(doc));
}

const char* CBLDocument_ID(const CBLDocument* doc) CBLAPI              {return doc->docID();}
uint64_t CBLDocument_Sequence(const CBLDocument* doc) CBLAPI           {return doc->sequence();}
FLDict CBLDocument_Properties(const CBLDocument* doc) CBLAPI           {return doc->properties();}
FLMutableDict CBLDocument_MutableProperties(CBLDocument* doc) CBLAPI   {return doc->mutableProperties();}
FLDoc CBLDocument_CreateFleeceDoc(const CBLDocument* doc) CBLAPI       {return doc->createFleeceDoc();}

char* CBLDocument_PropertiesAsJSON(const CBLDocument* doc) CBLAPI      {return doc->propertiesAsJSON();}

void CBLDocument_SetProperties(CBLDocument* doc, FLMutableDict properties _cbl_nonnull) CBLAPI {
    doc->setProperties(properties);
}

bool CBLDocument_SetPropertiesAsJSON(CBLDocument* doc, const char *json, CBLError* outError) CBLAPI {
    return doc->setPropertiesAsJSON(json, internal(outError));
}

const CBLDocument* CBLDatabase_SaveDocument(CBLDatabase* db,
                                       CBLDocument* doc,
                                       CBLConcurrencyControl concurrency,
                                       CBLError* outError) CBLAPI
{
    return retain(doc->save(db, {concurrency}, internal(outError)).get());
}

const CBLDocument* CBLDatabase_SaveDocumentResolving(CBLDatabase* db _cbl_nonnull,
                                                       CBLDocument* doc _cbl_nonnull,
                                                       CBLSaveConflictHandler conflictHandler,
                                                       void *context,
                                                       CBLError* outError) CBLAPI
{
    return retain(doc->save(db, {conflictHandler, context}, internal(outError)).get());
}

bool CBLDocument_Delete(const CBLDocument* doc _cbl_nonnull,
                        CBLConcurrencyControl concurrency,
                        CBLError* outError) CBLAPI
{
    return const_cast<CBLDocument*>(doc)->deleteDoc(concurrency, internal(outError));
}

bool CBLDatabase_DeleteDocumentByID(CBLDatabase* db _cbl_nonnull,
                               const char* docID _cbl_nonnull,
                               CBLError* outError) CBLAPI
{
    return CBLDocument::deleteDoc(db, docID, internal(outError));
}

bool CBLDocument_Purge(const CBLDocument* doc _cbl_nonnull,
                   CBLError* outError) CBLAPI
{
    return CBLDatabase_PurgeDocumentByID(doc->database(), doc->docID(), outError);
}

bool CBLDatabase_PurgeDocumentByID(CBLDatabase* db _cbl_nonnull,
                              const char* docID _cbl_nonnull,
                              CBLError* outError) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        c4::Transaction t(c4db);
        return t.begin(internal(outError))
            && c4db_purgeDoc(c4db, slice(docID), internal(outError))
            && t.commit(internal(outError));
    });
}

CBLTimestamp CBLDatabase_GetDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                         const char *docID _cbl_nonnull,
                                         CBLError* error) CBLAPI
{
    return db->use<CBLTimestamp>([&](C4Database *c4db) {
        return c4doc_getExpiration(c4db, slice(docID), internal(error));
    });
}

bool CBLDatabase_SetDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                       const char *docID _cbl_nonnull,
                                       CBLTimestamp expiration,
                                       CBLError* error) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        return c4doc_setExpiration(c4db, slice(docID), expiration, internal(error));
    });
}
