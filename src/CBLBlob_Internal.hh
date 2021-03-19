//
// CBLBlob_Internal.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
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
#include "CBLBlob.h"
#include "CBLLog.h"
#include "CBLDatabase_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "Internal.hh"
#include "Util.hh"
#include "c4BlobStore.hh"
#include "c4Document.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <mutex>


static inline C4ReadStream*  internal(CBLBlobReadStream *reader)  {return (C4ReadStream*)reader;}
static inline C4WriteStream* internal(CBLBlobWriteStream *writer) {return (C4WriteStream*)writer;}


struct CBLBlob : public CBLRefCounted {
public:
    // Constructor for existing blobs -- called by CBLDocument::getBlob()
    CBLBlob(CBLDocument *doc, Dict properties)
    :_db(doc->database())
    ,_properties( (_db && C4Blob::isBlob(properties, _key)) ? properties : nullptr)
    { }

    bool valid() const              {return _properties != nullptr;}
    Dict properties() const         {return _properties;}

    alloc_slice propertiesAsJSON() const {
        if (!_properties)
            return fleece::nullslice;
        fleece::JSONEncoder enc;
        enc.writeValue(_properties);
        return enc.finish();
    }

    uint64_t contentLength() const {
        Value len = _properties[kCBLBlobLengthProperty];
        if (len.isInteger())
            return len.asUnsigned();
        else
            return store()->getSize(_key);
    }

    slice digest() const {
        LOCK(_mutex);
        if (!_digest)
            _digest = C4Blob::keyToString(_key);
        return _digest;
    }

    slice contentType() const {
        slice type = _properties[kCBLBlobContentTypeProperty].asString();
        if (!type)
            return fleece::nullslice;
        LOCK(_mutex);
        if (type != _contentTypeCache)
            _contentTypeCache = type;
        return _contentTypeCache;
    }

    virtual alloc_slice getContents() const {
        return store()->getContents(_key);
    }

    virtual std::unique_ptr<C4ReadStream> openStream() const {
        return std::make_unique<C4ReadStream>(*store(), _key);
    }

    virtual bool install(CBLDatabase *db _cbl_nonnull) {
        return true;
    }

protected:
    // constructor for CBLNewBlob to call
    CBLBlob()
    :_mutableProperties(MutableDict::newDict())
    ,_properties(_mutableProperties)
    { }

    CBLDatabase* database() const                       {return _db;}
    void setDatabase(CBLDatabase *db _cbl_nonnull)      {assert(!_db); _db = db;}

    C4BlobStore* store() const {
        assert(_db);
        return _db->blobStore();
    }

    MutableDict mutableProperties()                     {return _mutableProperties;}

    const C4BlobKey& key() const                        {return _key;}
    void setKey(const C4BlobKey &key)                   {_key = key;}

    mutable std::mutex     _mutex;

private:
    bool findDatabase() {
        assert(!_db);
        const CBLDocument* doc = CBLDocument::containing(_properties);
        if (doc)
            _db = doc->database();
        return (_db != nullptr);
    }

    CBLDatabase*        _db {nullptr};
    MutableDict const   _mutableProperties;
    C4BlobKey           _key {};
    Dict const          _properties;
    mutable alloc_slice _digest;
    mutable alloc_slice _contentTypeCache;
};


struct CBLNewBlob : public CBLBlob {
public:
    // Constructor for new blobs, given contents or writer (but not both)
    CBLNewBlob(slice contentType,
               slice contents,
               C4WriteStream *writerOrNull)
    :CBLBlob()
    {
        assert(mutableProperties() != nullptr);
        mutableProperties()[kCBLTypeProperty] = kCBLBlobType;
        if (contentType)
            mutableProperties()[kCBLBlobContentTypeProperty] = contentType;

        C4BlobKey key;
        uint64_t length;
        if (contents) {
            assert(!writerOrNull);
            _contents = contents;
            key = C4Blob::computeKey(contents);
            length = contents.size;
        } else {
            assert(writerOrNull);
            _writer.emplace(std::move(*writerOrNull));
            key = _writer->computeBlobKey();
            length = _writer->bytesWritten();
        }
        setKey(key, length);
        CBLDocument::registerNewBlob(this);
    }

    virtual alloc_slice getContents() const override {
        if (database())
            return CBLBlob::getContents();

        LOCK(_mutex);
        if (!_contents) {
            C4Error::raise(LiteCoreDomain, kC4ErrorNotFound,
                           "Can't get streamed blob contents until doc is saved");
        }
        return _contents;
    }

    virtual std::unique_ptr<C4ReadStream> openStream() const override {
        if (!database()) {
            C4Error::raise(LiteCoreDomain, kC4ErrorNotFound,
                           "Can't stream blob contents until doc is saved");
        }
        return CBLBlob::openStream();
    }

    virtual bool install(CBLDatabase *db _cbl_nonnull) override {
        CBL_Log(kCBLLogDomainDatabase, CBLLogInfo, "Saving new blob '%.*s'", FMTSLICE(digest()));
        LOCK(_mutex);
        assert(database() == nullptr || database() == db);
        const C4BlobKey &expectedKey = key();
        if (_contents) {
            db->blobStore()->createBlob(_contents, &expectedKey);
            _contents = fleece::nullslice;
        } else {
            assert(_writer);
            _writer->install(&expectedKey);
            _writer = std::nullopt;
        }
        setDatabase(db);
        CBLDocument::unregisterNewBlob(this);
        return true;
    }

protected:
    ~CBLNewBlob() {
        if (!database())
            CBLDocument::unregisterNewBlob(this);
    }

private:
    void setKey(const C4BlobKey &key, uint64_t length) {
        CBLBlob::setKey(key);
        mutableProperties()[kCBLBlobDigestProperty] = digest();
        mutableProperties()[kCBLBlobLengthProperty] = length;
    }

    alloc_slice     _contents;              // Contents, before save
    std::optional<C4WriteStream>  _writer ;      // Stream, before save
};
