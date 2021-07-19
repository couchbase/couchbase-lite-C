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
#include <mutex>
#include "betterassert.hh"

using namespace std;
using namespace fleece;
using namespace cbl_internal;


// The methods below cannot be in C4Document_Internal.hh because they depend on
// CBLDatabase_Internal.hh, which would create a circular header dependency.


CBLDocument::CBLDocument(slice docID, CBLDatabase *db, C4Document *c4doc, bool isMutable)
:_docID(docID)
,_db(db)
,_c4doc(c4doc)
,_mutable(isMutable)
{
    if (c4doc) {
        c4doc->extraInfo() = {this, nullptr};
        _revID = c4doc->selectedRev().revID;
    }
}


CBLDocument::~CBLDocument() {
    auto c4doc = _c4doc.useLocked();
    if (c4doc)
        c4doc->extraInfo() = {nullptr, nullptr};
}


#pragma mark - SAVING:


bool CBLDocument::save(CBLDatabase* db, const SaveOptions &opt) {
    auto c4doc = _c4doc.useLocked();

    if (opt.deleting) {
        if (!c4doc)
            C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Document is not in any database");
    } else {
        checkMutable();
    }
    checkDBMatches(_db, db);

    Retained<C4Document> newDoc = nullptr;
    db->useLocked([&](C4Database *c4db) {
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
        Retained<C4Document> savingDoc = c4doc.get();

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
                    if (conflictingDoc) {
                        savingDoc = const_cast<C4Document*>(conflictingDoc->_c4doc.useLocked().get());
                    } else {
                        savingDoc = nullptr;
                    }
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
    c4doc.get() = move(newDoc);
    _revID = c4doc->selectedRev().revID;
    return true;
}


alloc_slice CBLDocument::encodeBody(CBLDatabase* db,
                                    C4Database* c4db,
                                    C4RevisionFlags &outRevFlags) const
{
    auto c4doc = _c4doc.useLocked();
    // Save new blobs:
    bool hasBlobs = saveBlobs(db);
    outRevFlags = hasBlobs ? kRevHasAttachments : 0;

    // Now encode the properties to Fleece:
    SharedEncoder enc(c4db->sharedFleeceEncoder());
    enc.writeValue(properties());
    FLError flErr;
    alloc_slice body = enc.finish(&flErr);
    if (!body)
        C4Error::raise(FleeceDomain, flErr);
    return body;
}


#pragma mark - CONFLICT RESOLUTION:


bool CBLDocument::resolveConflict(Resolution resolution, const CBLDocument * _cbl_nullable resolveDoc) {
    auto c4doc = _c4doc.useLocked();

    if (!c4doc)
        C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Document has not been saved to a database");

    _properties = nullptr;
    _fromJSON = nullptr;

    // Remote Revision always win so that the resolved revision will not conflict with the remote:
    slice winner(c4doc->selectedRev().revID), loser(c4doc->revID());
    
    return _db->useLocked<bool>([&](C4Database *c4db) {
        C4Database::Transaction t(c4db);

        alloc_slice mergeBody;
        C4RevisionFlags mergeFlags = 0;
        if (resolution != Resolution::useRemote) {
            if (resolveDoc) {
                mergeBody = resolveDoc->encodeBody(_db, c4db, mergeFlags);
            } else {
                mergeBody = alloc_slice(size_t(0));
                mergeFlags = kRevDeleted;
            }
        }

        try {
            c4doc->resolveConflict(winner, loser, mergeBody, mergeFlags);
        } catch (...) {
            C4Error err = C4Error::fromCurrentException();
            if (err == C4Error{LiteCoreDomain, kC4ErrorConflict})
                return false;
            throw;
        }

        c4doc->save();
        t.commit();
        return true;
    });
}


#pragma mark - BLOBS:


using UnretainedValueToBlobMap = litecore::access_lock<std::unordered_map<FLDict, CBLNewBlob*>>;

static UnretainedValueToBlobMap& newBlobs() {
    static UnretainedValueToBlobMap sNewBlobs;
    return sNewBlobs;
}


void CBLDocument::registerNewBlob(CBLNewBlob* blob) {
    newBlobs().useLocked()->insert({blob->properties(), blob});
}


void CBLDocument::unregisterNewBlob(CBLNewBlob* blob) {
    newBlobs().useLocked().get().erase(blob->properties());
}


CBLNewBlob* CBLDocument::findNewBlob(FLDict dict) {
    return newBlobs().useLocked<CBLNewBlob*>([dict](auto &newBlobs) -> CBLNewBlob* {
        auto i = newBlobs.find(dict);
        if (i == newBlobs.end())
            return nullptr;
        return i->second;
    });
}


CBLBlob* CBLDocument::getBlob(FLDict dict) {
    auto c4doc = _c4doc.useLocked();
    // Is it already registered by a previous call to getBlob?
    if (auto i = _blobs.find(dict); i != _blobs.end())
        return i->second;
    // Is it a NewBlob?
    if (Dict(dict).asMutable()) {
        CBLNewBlob *newBlob = findNewBlob(dict);
        if (newBlob)
            return newBlob;
    }

    // Not found; is it a blob or attachment at all?
    auto key = C4Blob::keyFromDigestProperty(dict);
    if (!key || !(C4Blob::isBlob(dict) || C4Blob::isAttachmentIn(dict, properties())))
        return nullptr;

    // Create a new CBLBlob and remember it:
    auto blob = retained(new CBLBlob(this, dict, *key));
    _blobs.insert({dict, blob});
    return blob;
}


bool CBLDocument::saveBlobs(CBLDatabase *db) const {
    // Walk through the Fleece object tree, looking for new mutable blob Dicts to install,
    // and also checking if there are any blobs at all (mutable or not.)
    // Once we've found at least one blob, we can skip immutable collections, because
    // they can't contain new blobs.
    auto c4doc = _c4doc.useLocked();
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
