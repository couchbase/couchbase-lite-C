//
// CBLQuery.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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

#include "CBLDatabase_Internal.hh"
#include "CBLBlob_Internal.hh"
#include "CBLQuery_Internal.hh"
#include "CBLEncryptable_Internal.hh"
#include "c4Log.h"


using namespace std;
using namespace fleece;


void ListenerToken<CBLQueryChangeListener>::setEnabled(bool enabled) {
    auto c4query = _query->_c4query.useLocked();
    
    if (enabled == _isEnabled)
        return;
    
    CBLDatabase* db = const_cast<CBLDatabase*>(_query->database());
    if (enabled) {
        if (!db->registerService(this, [this] { this->setEnabled(false); })) {
            CBL_Log(kCBLLogDomainQuery, kCBLLogWarning,
                    "Couldn't enable the Query Listener as the database is closing or closed.");
            return;
        }
    }
    
    _c4obs->setEnabled(enabled);
    _isEnabled = enabled;
    
    if (!enabled) {
        db->unregisterService(this);
    }
}


CBLResultSet::CBLResultSet(CBLQuery* query, C4Query::Enumerator qe)
:_query(query)
,_enum(std::move(qe))
{ }


CBLResultSet::~CBLResultSet() {
    if (_fleeceDoc)
        _fleeceDoc.setAssociated(nullptr, "CBLResultSet");
}


bool CBLResultSet::next() {
    _asArray = nullptr;
    _asDict = nullptr;
    _blobs.clear();
#ifdef COUCHBASE_ENTERPRISE
    _encryptables.clear();
#endif
    
    if (_enum.next()) {
        if (!_fleeceDoc) {
            // As soon as I read the first row, associate myself with the `Doc` backing the Fleece
            // data, so that the `getBlob()` method can find me.
            if (Value v = column(0); v) {
                _fleeceDoc = Doc::containing(v);
                if (!_fleeceDoc.setAssociated(this, "CBLResultSet"))
                    C4Warn("Couldn't associate CBLResultSet with FLDoc %p", FLDoc(_fleeceDoc));
            }
        }
        return true;
    } else {
        _fleeceDoc = nullptr;
        return false;
    }
}


Value CBLResultSet::property(slice prop) const {
    int col = _query->columnNamed(prop);
    return (col >= 0) ? column(col) : nullptr;
}


Array CBLResultSet::asArray() const {
    if (!_asArray) {
        auto array = MutableArray::newArray();
        unsigned nCols = _query->columnCount();
        array.resize(uint32_t(nCols));
        for (unsigned i = 0; i < nCols; ++i) {
            Value val = column(i);
            array[i] = val ? val : Value(kFLUndefinedValue);
        }
        _asArray = array;
    }
    return _asArray;
}


Dict CBLResultSet::asDict() const {
    if (!_asDict) {
        auto dict = MutableDict::newDict();
        unsigned nCols = _query->columnCount();
        for (unsigned i = 0; i < nCols; ++i) {
            if (Value val = column(i); val) {
                slice key = _query->columnName(i);
                dict[key] = val;
            }
        }
        _asDict = dict;
    }
    return _asDict;
}


Retained<CBLResultSet> CBLResultSet::containing(Value v) {
    return (CBLResultSet*) Doc::containing(v).associated("CBLResultSet");
}


CBLBlob* CBLResultSet::getBlob(Dict blobDict, const C4BlobKey &key) {
    // OK, let's find or create a CBLBlob, then cache it.
    // (It's not really necessary to cache the CBLBlobs -- they're lightweight objects --
    // but otherwise we'd have to return a `Retained<CBLBlob>`, which would complicate
    // the public C API by making the caller release it afterwards.)
    auto i = _blobs.find(blobDict);
    if (i == _blobs.end()) {
        auto db = const_cast<CBLDatabase*>(query()->database());
        i = _blobs.emplace(blobDict, new CBLBlob(db, blobDict, key)).first;
    }
    return i->second;
}


#ifdef COUCHBASE_ENTERPRISE

CBLEncryptable* CBLResultSet::getEncryptableValue(Dict encDict) {
    // Find or create a CBLEncryptable, then cache it.
    auto i = _encryptables.find(encDict);
    if (i == _encryptables.end()) {
        i = _encryptables.emplace(encDict, new CBLEncryptable(encDict)).first;
    }
    return i->second;
}

#endif
