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
using namespace cbl_internal;


static alloc_slice generateID() {
    char docIDBuf[32];
    return alloc_slice(c4doc_generateID(docIDBuf, sizeof(docIDBuf)));
}


static C4Document* getC4Doc(CBLDatabase *db, slice docID, bool allRevisions) {
    return db->use<C4Document*>([&](C4Database *c4db) {
        C4Document *doc = c4db_getDoc(c4db, docID,
                                      true, // mustExist
                                      (allRevisions ? kDocGetAll : kDocGetCurrentRev),
                                      nullptr);
        if (!allRevisions && doc && doc->flags & kDocDeleted) {
            c4doc_release(doc);
            doc = nullptr;
        }
        return doc;
    });
}


// Core constructor
CBLDocument::CBLDocument(slice docID,
                         CBLDatabase *db,
                         C4Document *d,          // must be a +1 ref, or null
                         bool isMutable)
:_docID(docID)
,_db(db)
,_c4doc(d)
,_mutable(isMutable)
{
    if (_c4doc) {
        _c4doc->extraInfo = {this, nullptr};
        _revID = _c4doc->selectedRev.revID;
    }
}


// Construct a new document (not in any database yet)
CBLDocument::CBLDocument(slice docID, bool isMutable)
:CBLDocument(docID ? docID : generateID(), nullptr, nullptr, isMutable)
{ }


// Construct on an existing document
CBLDocument::CBLDocument(CBLDatabase *db, slice docID, bool isMutable, bool allRevisions)
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
                         slice docID,
                         slice revID,
                         C4RevisionFlags revFlags,
                         Dict body)
:CBLDocument(docID, db, nullptr, false)
{
    _properties = body;
    _revID = revID;
}


CBLDocument::~CBLDocument() {
}


C4RevisionFlags CBLDocument::revisionFlags() const {
    return _c4doc ? _c4doc->selectedRev.flags : (kRevNew | kRevLeaf);
}


bool CBLDocument::checkMutable(C4Error *outError) const {
    if (_usuallyTrue(_mutable))
        return true;
    setError(outError, LiteCoreDomain, kC4ErrorNotWriteable, "Document object is immutable"_sl);
    return false;
}


#pragma mark - SAVING / DELETING:


static bool checkDBMatches(CBLDatabase *myDB, CBLDatabase *dbParam _cbl_nonnull, C4Error *outError) {
    if (myDB && myDB != dbParam) {
        setError(outError, LiteCoreDomain, kC4ErrorInvalidParameter,
                 "Saving doc to wrong database"_sl);
        return false;
    } else {
        return true;
    }
}


bool CBLDocument::save(CBLDatabase* db _cbl_nonnull,
                       const SaveOptions &opt,
                       C4Error* outError)
{
    LOCK(_mutex);

    if ((!opt.deleting && !checkMutable(outError)) || !checkDBMatches(_db, db, outError))
        return false;

    c4::ref<C4Document> newDoc = nullptr;
    db->use([&](C4Database *c4db) {
        c4::Transaction t(c4db);
        if (!t.begin(outError))
            return;

        // Encode properties:
        alloc_slice body;
        C4RevisionFlags revFlags;
        if (!opt.deleting) {
            body = encodeBody(db, c4db, revFlags, outError);
            if (!body)
                return;
        } else {
            revFlags = kRevDeleted;
        }

        // Save:
        c4::ref<C4Document> savingDoc = c4doc_retain(_c4doc);
        C4Error c4err;

        bool retrying;
        do {
            retrying = false;
            if (savingDoc) {
                // Update existing doc:
                newDoc = c4doc_update(savingDoc, body, revFlags, &c4err);
            } else {
                // Create new doc:
                C4DocPutRequest rq = {};
                rq.allocedBody = {body.buf, body.size};
                rq.docID = _docID;
                rq.revFlags = revFlags;
                rq.save = true;
                newDoc = c4doc_put(c4db, &rq, nullptr, &c4err);
            }

            if (!newDoc && c4err == C4Error{LiteCoreDomain, kC4ErrorConflict}) {
                // Conflict!
                if (opt.conflictHandler) {
                    // Custom conflict resolution:
                    auto conflictingDoc = make_nothrow<CBLDocument>(external(outError),
                                                                    db, _docID, false);
                    if (!conflictingDoc)
                        return;
                    if (!conflictingDoc->exists())
                        conflictingDoc = nullptr;
                    if (!opt.conflictHandler(opt.context, this, conflictingDoc)) {
                        c4err = {LiteCoreDomain, kC4ErrorConflict};
                        break;
                    }
                    body = encodeBody(db, c4db, revFlags, &c4err);
                    if (!body)
                        break;
                    savingDoc = conflictingDoc ? c4doc_retain(conflictingDoc->_c4doc) : nullptr;
                    retrying = true;
                } else if (opt.concurrency == kCBLConcurrencyControlLastWriteWins) {
                    // Last-write-wins; load current revision and retry:
                    savingDoc = c4db_getDoc(c4db, _docID, true, kDocGetCurrentRev, &c4err);
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
        return false;

    // Update my C4Document:
    _db = db;
    _c4doc = move(newDoc);
    _revID = _c4doc->selectedRev.revID;
    return true;
}


bool CBLDocument::deleteDoc(CBLDatabase *db _cbl_nonnull,
                            CBLConcurrencyControl concurrency,
                            C4Error* outError)
{
    if (!_c4doc) {
        setError(outError, LiteCoreDomain, kC4ErrorNotFound, "Document is not in any database"_sl);
        return false;
    }
    SaveOptions opt(concurrency);
    opt.deleting = true;
    return save(db, opt, outError);
}


/*static*/ bool CBLDocument::deleteDoc(CBLDatabase* db _cbl_nonnull,
                                       slice docID,
                                       C4Error* outError)
{
    return db->use<bool>([&](C4Database *c4db) {
        c4::Transaction t(c4db);
        if (!t.begin(outError))
            return false;
        c4::ref<C4Document> c4doc = c4db_getDoc(c4db, docID, false, kDocGetCurrentRev, outError);
        if (c4doc)
            c4doc = c4doc_update(c4doc, nullslice, kRevDeleted, outError);
        return c4doc && t.commit(outError);
    });
}


#pragma mark - CONFLICT RESOLUTION:


bool CBLDocument::selectRevision(slice revID) {
    LOCK(_mutex);
    if (!_c4doc || !c4doc_selectRevision(_c4doc, revID, true, nullptr))
        return false;
    _revID = revID;
    _properties = nullptr;
    _fromJSON = nullptr;
    return true;
}


bool CBLDocument::selectNextConflictingRevision() {
    LOCK(_mutex);
    if (!_c4doc)
        return false;
    _properties = nullptr;
    _fromJSON = nullptr;
    while (c4doc_selectNextLeafRevision(_c4doc, true, true, nullptr))
        if (_c4doc->selectedRev.flags & kRevIsConflict)
            return true;
    return false;
}


bool CBLDocument::resolveConflict(Resolution resolution,
                                  const CBLDocument *mergeDoc,
                                  CBLError* outError)
{
    LOCK(_mutex);
    C4Error *c4err = internal(outError);

    if (!_c4doc) {
        setError(c4err, LiteCoreDomain, kC4ErrorNotFound,
                 "Document has not been saved to a database"_sl);
        return false;
    }

    _properties = nullptr;
    _fromJSON = nullptr;

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
                mergeBody = mergeDoc->encodeBody(_db, c4db, mergeFlags, c4err);
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


FLDoc CBLDocument::createFleeceDoc() const {
#if 1 // TODO: RE-enable this
    return nullptr;
#else
    if (_c4doc)
        return c4doc_createFleeceDoc(_c4doc);
    else
        return FLDoc_FromJSON("{}"_sl, nullptr);
#endif
}


Dict CBLDocument::properties() const {
    //TODO: Convert this to use c4doc_getProperties()
    LOCK(_mutex);
    if (!_properties) {
        slice storage;
        if (_fromJSON)
            storage = _fromJSON.data();
        else if (_c4doc)
            storage = c4doc_getRevisionBody(_c4doc);
        
        if (storage)
            _properties = Value::fromData(storage);
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


alloc_slice CBLDocument::propertiesAsJSON() const {
    LOCK(_mutex);
    if (!_mutable && _c4doc)
        return c4doc_bodyAsJSON(_c4doc, false, nullptr);        // fast path
    else
        return properties().toJSON();
}


bool CBLDocument::setPropertiesAsJSON(slice json, C4Error* outError) {
    if (!checkMutable(outError))
        return false;
    Doc fromJSON = Doc::fromJSON(json);
    if (!fromJSON) {
        setError(outError, FleeceDomain, kFLJSONError, "Invalid JSON"_sl);
        return false;
    }
    LOCK(_mutex);
    // Store the transcoded Fleece and clear _properties. If app accesses properties(),
    // it'll get a mutable version of this.
    _fromJSON = fromJSON;
    _properties = nullptr;
    return true;
}


alloc_slice CBLDocument::encodeBody(CBLDatabase *db _cbl_nonnull,
                                    C4Database *c4db _cbl_nonnull,
                                    C4RevisionFlags &outRevFlags,
                                    C4Error *outError) const
{
    LOCK(_mutex);
    // Save new blobs:
    bool hasBlobs;
    if (!saveBlobs(db, hasBlobs, outError))
        return nullslice;
    outRevFlags = hasBlobs ? kRevHasAttachments : 0;

    // Now encode the properties to Fleece:
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
    auto blob = make_nothrow<CBLBlob>(nullptr, this, dict);
    if (!blob || !blob->valid())
        return nullptr;
    _blobs.insert({dict, blob});
    return blob;
}


void CBLDocument::registerNewBlob(CBLNewBlob* blob) {
    LOCK(sNewBlobsMutex);
    if (!sNewBlobs) {
        sNewBlobs = new (nothrow) UnretainedValueToBlobMap;
        postcondition(sNewBlobs != nullptr);
    }
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


bool CBLDocument::saveBlobs(CBLDatabase *db, bool &outHasBlobs, C4Error *outError) const {
    // Walk through the Fleece object tree, looking for new mutable blob Dicts to install,
    // and also checking if there are any blobs at all (mutable or not.)
    // Once we've found at least one blob, we can skip immutable collections, because
    // they can't contain new blobs.
    LOCK(_mutex);
    if (!isMutable()) {
        outHasBlobs = c4doc_dictContainsBlobs(properties());
        return true;
    }
    bool foundBlobs = false;
    for (DeepIterator i(properties()); i; ++i) {
        Dict dict = i.value().asDict();
        if (dict) {
            if (!dict.asMutable()) {
                if (!foundBlobs)
                    foundBlobs = FLDict_IsBlob(dict);
                if (foundBlobs)
                    i.skipChildren();
            } else if (FLDict_IsBlob(dict)) {
                foundBlobs = true;
                CBLNewBlob *newBlob = findNewBlob(dict);
                if (newBlob) {
                    if (!newBlob->install(db, outError))
                        return false;
                }
                i.skipChildren();
            }
        } else if (!i.value().asArray().asMutable()) {
            if (foundBlobs)
                i.skipChildren();
        }
    }
    outHasBlobs = foundBlobs;
    return true;
}


#pragma mark - PUBLIC API:


static CBLDocument* getDocument(CBLDatabase* db, slice docID, bool isMutable) CBLAPI {
    auto doc = make_nothrow<CBLDocument>(nullptr, db, docID, isMutable);
    if (!doc || !doc->exists())
        return nullptr;
    return move(doc).detach();
}

const CBLDocument* CBLDatabase_GetDocument(const CBLDatabase* db, FLString docID) CBLAPI {
    return getDocument((CBLDatabase*)db, docID, false);
}

CBLDocument* CBLDatabase_GetMutableDocument(CBLDatabase* db, FLString docID) CBLAPI {
    return getDocument(db, docID, true);
}

CBLDocument* CBLDocument_New() CBLAPI {
    return CBLDocument_NewWithID(nullslice);
}

CBLDocument* CBLDocument_NewWithID(FLString docID) CBLAPI {
    return make_nothrow<CBLDocument>(nullptr, docID, true).detach();
}

CBLDocument* CBLDocument_ToMutable(const CBLDocument* doc) CBLAPI {
    return make_nothrow<CBLDocument>(nullptr, doc).detach();
}

FLSlice CBLDocument_ID(const CBLDocument* doc) CBLAPI              {return doc->docID();}
FLSlice CBLDocument_RevisionID(const CBLDocument* doc) CBLAPI      {return doc->revisionID();}
uint64_t CBLDocument_Sequence(const CBLDocument* doc) CBLAPI           {return doc->sequence();}
FLDict CBLDocument_Properties(const CBLDocument* doc) CBLAPI           {return doc->properties();}
FLMutableDict CBLDocument_MutableProperties(CBLDocument* doc) CBLAPI   {return doc->mutableProperties();}
FLDoc CBLDocument_ToFleeceDoc(const CBLDocument* doc) CBLAPI       {return doc->createFleeceDoc();}

FLSliceResult CBLDocument_ToJSON(const CBLDocument* doc) CBLAPI {return FLSliceResult(doc->propertiesAsJSON());}

void CBLDocument_SetProperties(CBLDocument* doc, FLMutableDict properties _cbl_nonnull) CBLAPI {
    doc->setProperties(properties);
}

bool CBLDocument_SetJSON(CBLDocument* doc, FLSlice json, CBLError* outError) CBLAPI {
    return doc->setPropertiesAsJSON(json, internal(outError));
}

bool CBLDatabase_SaveDocument(CBLDatabase* db,
                              CBLDocument* doc,
                              CBLError* outError) CBLAPI
{
    return CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc,
                                                          kCBLConcurrencyControlLastWriteWins,
                                                          outError);
}

bool CBLDatabase_SaveDocumentWithConcurrencyControl(CBLDatabase* db,
                              CBLDocument* doc,
                              CBLConcurrencyControl concurrency,
                              CBLError* outError) CBLAPI
{
    return doc->save(db, {concurrency}, internal(outError));
}

bool CBLDatabase_SaveDocumentWithConflictHandler(CBLDatabase* db _cbl_nonnull,
                                       CBLDocument* doc _cbl_nonnull,
                                       CBLConflictHandler conflictHandler,
                                       void *context,
                                       CBLError* outError) CBLAPI
{
    return doc->save(db, {conflictHandler, context}, internal(outError));
}

bool CBLDatabase_DeleteDocument(CBLDatabase *db _cbl_nonnull,
                                const CBLDocument* doc _cbl_nonnull,
                                CBLError* outError) CBLAPI
{
    return CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc,
                                                            kCBLConcurrencyControlLastWriteWins,
                                                            outError);
}

bool CBLDatabase_DeleteDocumentWithConcurrencyControl(CBLDatabase *db _cbl_nonnull,
                                                      const CBLDocument* doc _cbl_nonnull,
                                                      CBLConcurrencyControl concurrency,
                                                      CBLError* outError) CBLAPI
{
    return const_cast<CBLDocument*>(doc)->deleteDoc(db, concurrency, internal(outError));
}

bool CBLDatabase_DeleteDocumentByID(CBLDatabase* db _cbl_nonnull,
                                    FLString docID,
                                    CBLError* outError) CBLAPI
{
    return CBLDocument::deleteDoc(db, docID, internal(outError));
}

bool CBLDatabase_PurgeDocument(CBLDatabase* db _cbl_nonnull,
                               const CBLDocument* doc _cbl_nonnull,
                               CBLError* outError) CBLAPI
{
    return CBLDatabase_PurgeDocumentByID(doc->database(), doc->docID(), outError);
}

bool CBLDatabase_PurgeDocumentByID(CBLDatabase* db,
                                   FLString docID,
                                   CBLError* outError) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        c4::Transaction t(c4db);
        return t.begin(internal(outError))
            && c4db_purgeDoc(c4db, docID, internal(outError))
            && t.commit(internal(outError));
    });
}

CBLTimestamp CBLDatabase_GetDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                               FLSlice docID,
                                               CBLError* error) CBLAPI
{
    return db->use<CBLTimestamp>([&](C4Database *c4db) {
        return c4doc_getExpiration(c4db, docID, internal(error));
    });
}

bool CBLDatabase_SetDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                       FLSlice docID,
                                       CBLTimestamp expiration,
                                       CBLError* error) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        if (!c4doc_setExpiration(c4db, docID, expiration, internal(error)))
            return false;
        c4db_startHousekeeping(c4db);
        return true;
    });
}
