//
// CBLQueryIndex_Internal.hh
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

#pragma once
#include "access_lock.hh"
#include "Internal.hh"
#include <mutex>
#include <unordered_map>

CBL_ASSUME_NONNULL_BEGIN

struct CBLQueryIndex final : public CBLRefCounted {
public:
    CBLQueryIndex(Retained<C4Index>&& index, CBLCollection* collection);
    
    CBLCollection* collection() const;
    
    slice name() const;
    
#ifdef COUCHBASE_ENTERPRISE
    Retained<struct CBLIndexUpdater> beginUpdate(size_t limit);
#endif
    
private:
    Retained<CBLCollection>                         _collection;
    litecore::shared_access_lock<Retained<C4Index>> _c4Index;
};

#ifdef COUCHBASE_ENTERPRISE

struct CBLIndexUpdater final : public CBLRefCounted {
public:
    CBLIndexUpdater(Retained<C4IndexUpdater>&& indexUpdater, CBLQueryIndex* index);
    
    ~CBLIndexUpdater();
    
    size_t count() const;
    
    FLValue _cbl_nullable value(size_t index);

    void setVector(size_t index, const float* _cbl_nullable vector, size_t dimension);

    void skipVector(size_t index);

    void finish();
    
protected:
    friend struct CBLBlob;
    
    /** For getting blobs object. */
    static Retained<CBLIndexUpdater> containing(Value v);
    
    CBLBlob* getBlob(Dict blobDict, const C4BlobKey&);
   
private:
    mutable std::mutex                                      _mutex;
    Retained<C4IndexUpdater>                                _c4IndexUpdater;
    litecore::shared_access_lock<Retained<C4IndexUpdater>>  _c4IndexUpdaterWithLock;
    Retained<CBLQueryIndex>                                 _index;
    
    Doc                                                     _fleeceDoc;    // Fleece Doc that owns the values
    std::unordered_map<FLDict, Retained<CBLBlob>>           _blobs;        // Cached CBLBLobs, keyed by FLDict
};

#endif

CBL_ASSUME_NONNULL_END
