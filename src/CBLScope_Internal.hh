//
//  CBLScope_Internal.hh
//
// Copyright (c) 2022 Couchbase, Inc All rights reserved.
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
#include "CBLScope.h"
#include "CBLDatabase_Internal.hh"
#include "CBLPrivate.h"

CBL_ASSUME_NONNULL_BEGIN

struct CBLScope final : public CBLRefCounted {
public:
    
#pragma mark - CONSTRUCTORS:
    
    CBLScope(slice name, CBLDatabase* database)
    :_name(name)
    ,_database(database)
    { }
    
#pragma mark - ACCESSORS:
    
    slice name() const noexcept             {return _name;}

#pragma mark - COLLECTIONS:
    
    fleece::MutableArray collectionNames() const {
        LOCK(_mutex);
        checkOpen();
        return _database->collectionNames(_name);
    }
    
    Retained<CBLCollection> getCollection(slice collectionName) const;
    
protected:
    
    friend struct CBLDatabase;
    
    // Need to call under the _mutex lock
    void checkOpen() const {
        if (!_database) {
            C4Error::raise(LiteCoreDomain, kC4ErrorNotOpen,
                           "Invalid scope: db closed or deleted");
        }
    }
    
    void close() {
        LOCK(_mutex);
        _database = nullptr;
    }
    
private:
    
    CBLDatabase* _cbl_nullable              _database;  // Not retain to prevent retain cycle
    alloc_slice const                       _name;      // Name (never empty)
    mutable std::mutex                      _mutex;
};

CBL_ASSUME_NONNULL_END
