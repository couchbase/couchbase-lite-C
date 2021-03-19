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
#include "c4BlobStore.hh"
#include "c4Private.h"
#include "Util.hh"
#include <mutex>

using namespace std;
using namespace fleece;
using namespace cbl_internal;


// Core constructor
CBLDocument::CBLDocument(slice docID,
                         CBLDatabase *db,
                         Retained<C4Document> c4doc,
                         bool isMutable)
:_docID(docID)
,_db(db)
,_c4doc(move(c4doc))
,_mutable(isMutable)
{
    if (_c4doc) {
        _c4doc->extraInfo() = {this, nullptr};
        _revID = _c4doc->selectedRev().revID;
    }
}


// Construct a new document (not in any database yet)
CBLDocument::CBLDocument(slice docID, bool isMutable)
:CBLDocument(docID ? docID : C4Document::createDocID(), nullptr, nullptr, isMutable)
{ }


// Mutable copy of another CBLDocument
CBLDocument::CBLDocument(const CBLDocument* otherDoc)
:CBLDocument(otherDoc->_docID,
             otherDoc->_db,
             otherDoc->_c4doc,
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
    if (_c4doc)
        _c4doc->extraInfo() = {nullptr, nullptr};
}


alloc_slice CBLDocument::canonicalRevisionID() const {
    if (!_c4doc)
        return nullslice;
    _c4doc->selectCurrentRevision();
    return _c4doc->getSelectedRevIDGlobalForm();
}


C4RevisionFlags CBLDocument::revisionFlags() const {
    return _c4doc ? _c4doc->selectedRev().flags : (kRevNew | kRevLeaf);
}


void CBLDocument::checkMutable() const {
    if (!_usuallyTrue(_mutable))
        C4Error::raise(LiteCoreDomain, kC4ErrorNotWriteable, "Document object is immutable");
}


#pragma mark - SAVING / DELETING:


static void checkDBMatches(CBLDatabase *myDB, CBLDatabase *dbParam _cbl_nonnull) {
    if (myDB && myDB != dbParam)
        C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter, "Saving doc to wrong database");
}


bool CBLDocument::save(CBLDatabase* db _cbl_nonnull, const SaveOptions &opt) {
    LOCK(_mutex);

    if (!opt.deleting)
        checkMutable();
    checkDBMatches(_db, db);

    Retained<C4Document> newDoc = nullptr;
    db->use([&](C4Database *c4db) {
        C4Database::Transaction t(c4db);

        // Encode properties:
        alloc_slice body;
        C4RevisionFlags revFlags;
        if (!opt.deleting) {
            body = encodeBody(db, c4db, revFlags);
        } else {
            revFlags = kRevDeleted;
        }

        // Save:
        Retained<C4Document> savingDoc = _c4doc;

        bool retrying;
        do {
            retrying = false;
            if (savingDoc) {
                // Update existing doc:
                newDoc = savingDoc->update(body, revFlags);
            } else {
                // Create new doc:
                C4DocPutRequest rq = {};
                rq.allocedBody = {body.buf, body.size};
                rq.docID = _docID;
                rq.revFlags = revFlags;
                rq.save = true;
                C4Error c4err;
                newDoc = c4db->putDocument(rq, nullptr, &c4err);
                if (!newDoc && c4err != C4Error{LiteCoreDomain, kC4ErrorConflict})
                    C4Error::raise(c4err);
            }

            if (!newDoc) {
                // Conflict!
                if (opt.conflictHandler) {
                    // Custom conflict resolution:
                    auto conflictingDoc = db->getDocument(_docID, true);
                    if (conflictingDoc && conflictingDoc->revisionFlags() & kRevDeleted)
                        conflictingDoc = nullptr;
                    if (!opt.conflictHandler(opt.context, this, conflictingDoc))
                        break;
                    body = encodeBody(db, c4db, revFlags);
                    savingDoc = conflictingDoc ? conflictingDoc->_c4doc : nullptr;
                    retrying = true;
                } else if (opt.concurrency == kCBLConcurrencyControlLastWriteWins) {
                    // Last-write-wins; load current revision and retry:
                    savingDoc = c4db->getDocument(_docID, true, kDocGetCurrentRev);
                    retrying = true;
                }
            }
        } while (retrying);

        if (newDoc)
            t.commit();
    });

    if (!newDoc)
        return false;

    // Update my C4Document:
    _db = db;
    _c4doc = move(newDoc);
    _revID = _c4doc->selectedRev().revID;
    return true;
}


bool CBLDocument::deleteDoc(CBLDatabase *db _cbl_nonnull,
                            CBLConcurrencyControl concurrency)
{
    if (!_c4doc)
        C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Document is not in any database");
    SaveOptions opt(concurrency);
    opt.deleting = true;
    return save(db, opt);
}


/*static*/ bool CBLDocument::deleteDoc(CBLDatabase* db _cbl_nonnull,
                                       slice docID)
{
    return db->use<bool>([&](C4Database *c4db) {
        C4Database::Transaction t(c4db);
        Retained<C4Document> c4doc = c4db->getDocument(docID, false, kDocGetCurrentRev);
        if (c4doc)
            c4doc = c4doc->update(nullslice, kRevDeleted);
        if (!c4doc)
            return false;
        t.commit();
        return true;
    });
}


#pragma mark - CONFLICT RESOLUTION:


bool CBLDocument::selectRevision(slice revID) {
    LOCK(_mutex);
    if (!_c4doc || !_c4doc->selectRevision(revID, true))
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
    while (_c4doc->selectNextLeafRevision(true, true))
        if (_c4doc->selectedRev().flags & kRevIsConflict)
            return true;
    return false;
}


bool CBLDocument::resolveConflict(Resolution resolution, const CBLDocument *mergeDoc) {
    LOCK(_mutex);

    if (!_c4doc)
        C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Document has not been saved to a database");

    _properties = nullptr;
    _fromJSON = nullptr;

    slice winner(_c4doc->selectedRev().revID), loser(_c4doc->revID());
    if (resolution != Resolution::useRemote)
        std::swap(winner, loser);

    return _db->use<bool>([&](C4Database *c4db) {
        C4Database::Transaction t(c4db);

        alloc_slice mergeBody;
        C4RevisionFlags mergeFlags = 0;
        if (resolution == Resolution::useMerge) {
            if (mergeDoc) {
                mergeBody = mergeDoc->encodeBody(_db, c4db, mergeFlags);
            } else {
                mergeBody = alloc_slice(size_t(0));
                mergeFlags = kRevDeleted;
            }
        } else {
            assert(!mergeDoc);
        }

        try {
            _c4doc->resolveConflict(winner, loser, mergeBody, mergeFlags);
        } catch (...) {
            C4Error err = C4Error::fromCurrentException();
            if (err == C4Error{LiteCoreDomain, kC4ErrorConflict})
                return false;
            throw;
        }

        _c4doc->save();
        t.commit();
        return true;
    });
}


#pragma mark - PROPERTIES:


FLDoc CBLDocument::createFleeceDoc() const {
#if 1 // TODO: RE-enable this
    return nullptr;
#else
    if (_c4doc)
        return _c4doc->createFleeceDoc();
    else
        return FLDoc_FromJSON("{}"_sl, nullptr);
#endif
}


Dict CBLDocument::properties() const {
    //TODO: Convert this to use C4Document::getProperties()
    LOCK(_mutex);
    if (!_properties) {
        slice storage;
        if (_fromJSON)
            storage = _fromJSON.data();
        else if (_c4doc)
            storage = _c4doc->getRevisionBody();
        
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
        return _c4doc->bodyAsJSON(false);        // fast path
    else
        return properties().toJSON();
}


void CBLDocument::setPropertiesAsJSON(slice json) {
    checkMutable();
    Doc fromJSON = Doc::fromJSON(json);
    if (!fromJSON)
        C4Error::raise(FleeceDomain, kFLJSONError, "Invalid JSON");
    LOCK(_mutex);
    // Store the transcoded Fleece and clear _properties. If app accesses properties(),
    // it'll get a mutable version of this.
    _fromJSON = fromJSON;
    _properties = nullptr;
}


alloc_slice CBLDocument::encodeBody(CBLDatabase *db _cbl_nonnull,
                                    C4Database *c4db _cbl_nonnull,
                                    C4RevisionFlags &outRevFlags) const
{
    LOCK(_mutex);
    // Save new blobs:
    bool hasBlobs = saveBlobs(db);
    outRevFlags = hasBlobs ? kRevHasAttachments : 0;

    // Now encode the properties to Fleece:
    SharedEncoder enc(c4db->getSharedFleeceEncoder());
    enc.writeValue(properties());
    FLError flErr;
    alloc_slice body = enc.finish(&flErr);
    if (!body)
        C4Error::raise(FleeceDomain, flErr);
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


bool CBLDocument::saveBlobs(CBLDatabase *db) const {
    // Walk through the Fleece object tree, looking for new mutable blob Dicts to install,
    // and also checking if there are any blobs at all (mutable or not.)
    // Once we've found at least one blob, we can skip immutable collections, because
    // they can't contain new blobs.
    LOCK(_mutex);
    if (!isMutable())
        return C4Blob::dictContainsBlobs(properties());

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
                if (newBlob)
                    newBlob->install(db);
                i.skipChildren();
            }
        } else if (!i.value().asArray().asMutable()) {
            if (foundBlobs)
                i.skipChildren();
        }
    }
    return foundBlobs;
}
