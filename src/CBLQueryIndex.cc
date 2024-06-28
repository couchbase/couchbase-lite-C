//
// CBLQueryIndex.cc
//
// Copyright © 2024 Couchbase. All rights reserved.
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

#include "CBLQueryIndex_Internal.hh"
#include "CBLBlob_Internal.hh"
#include "CBLCollection_Internal.hh"
#include "c4Index.hh"

using namespace fleece;

#pragma mark - CBLQueryIndex:

CBLQueryIndex::CBLQueryIndex(Retained<C4Index>&& index, CBLCollection* collection)
:_collection(collection)
,_c4Index(std::move(index), _collection->database()->c4db().get())
{ }

CBLCollection* CBLQueryIndex::collection() const {
    return _collection;
}

slice CBLQueryIndex::name() const {
    return _c4Index.useLocked()->getName();
}

#ifdef COUCHBASE_ENTERPRISE

Retained<CBLIndexUpdater> CBLQueryIndex::beginUpdate(size_t limit) {
    auto updater = _c4Index.useLocked()->beginUpdate(limit);
    if (!updater) {
        return nullptr;
    }
    return new CBLIndexUpdater(std::move(updater), this->collection()->database());
}

#pragma mark - CBLIndexUpdater:

static const char* kCBLIndexUpdaterName = "CBLIndexUpdater";

CBLIndexUpdater::CBLIndexUpdater(Retained<C4IndexUpdater>&& indexUpdater, CBLDatabase* db)
:_c4IndexUpdater(indexUpdater)
,_db(db)
{  }

CBLIndexUpdater::~CBLIndexUpdater() {
    if (_fleeceDoc) {
        _fleeceDoc.setAssociated(nullptr, kCBLIndexUpdaterName);
    }
}

size_t CBLIndexUpdater::count() const {
    LOCK(_mutex);
    
    checkFinishedUnLock();
    
    return _c4IndexUpdater->count();
}

FLValue CBLIndexUpdater::value(size_t index) {
    LOCK(_mutex);
    
    checkFinishedUnLock();
    precondition(index < _c4IndexUpdater->count());
    
    auto val = _c4IndexUpdater->valueAt(index);
    
    // Associate myself with the `Doc` backing the Fleece
    // data, so that the `getBlob()` method can find me.
    if (!_fleeceDoc && val) {
        _fleeceDoc = Doc::containing(val);
        if (!_fleeceDoc.setAssociated(this, kCBLIndexUpdaterName))
            C4Warn("Couldn't associate CBLIndexUpdater with FLDoc %p", FLDoc(_fleeceDoc));
    }
    
    return val;
}

void CBLIndexUpdater::setVector(size_t index, const float* _cbl_nullable vector, size_t dimension) {
    LOCK(_mutex);
    
    checkFinishedUnLock();
    precondition(index < _c4IndexUpdater->count());
    
    _c4IndexUpdater->setVectorAt(index, vector, dimension);
}

void CBLIndexUpdater::skipVector(size_t index) {
    LOCK(_mutex);
    
    checkFinishedUnLock();
    precondition(index < _c4IndexUpdater->count());
    
    _c4IndexUpdater->skipVectorAt(index);
}

void CBLIndexUpdater::finish() {
    LOCK(_mutex);
    
    checkFinishedUnLock();
    
    auto lock = _db->c4db()->useLocked();
    _c4IndexUpdater->finish(); // Ignore return value
    _c4IndexUpdater = nullptr;
}

inline void CBLIndexUpdater::checkFinishedUnLock() const {
    if (!_c4IndexUpdater) {
        C4Error::raise(LiteCoreDomain, kC4ErrorNotOpen, "The index updater has already finished.");
    }
}

Retained<CBLIndexUpdater> CBLIndexUpdater::containing(Value v) {
    return (CBLIndexUpdater*) Doc::containing(v).associated(kCBLIndexUpdaterName);
}

CBLBlob* CBLIndexUpdater::getBlob(Dict blobDict, const C4BlobKey &key) {
    // OK, let's find or create a CBLBlob, then cache it.
    // (It's not really necessary to cache the CBLBlobs -- they're lightweight objects --
    // but otherwise we'd have to return a `Retained<CBLBlob>`, which would complicate
    // the public C API by making the caller release it afterwards.)
    LOCK(_mutex);
    auto i = _blobs.find(blobDict);
    if (i == _blobs.end()) {
        i = _blobs.emplace(blobDict, new CBLBlob(_db.get(), blobDict, key)).first;
    }
    return i->second;
}

#endif
