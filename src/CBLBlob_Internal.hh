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

#include "CBLBlob.h"
#include "CBLDatabase_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "Internal.hh"
#include "Util.hh"
#include "c4BlobStore.h"
#include "c4Document+Fleece.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"

using namespace std;
using namespace fleece;




static inline C4WriteStream* internal(CBLBlobWriteStream *writer) {return (C4WriteStream*)writer;}


class CBLBlob : public CBLRefCounted {
public:
    // Constructor for existing blobs -- called by CBLDocument::getBlob()
    CBLBlob(CBLDocument *doc, Dict properties)
    :_db(doc->database())
    {
        slice digest = properties[kCBLBlobDigestProperty].asString();
        if (_db && cbl_isBlob(properties) && c4blob_keyFromString(digest, &_key)) {
            _properties = properties.asMutable();
            if (!_properties)
                _properties = properties.mutableCopy();
        }
    }

    bool valid() const              {return _properties != nullptr;}
    Dict properties() const         {return _properties;}
    MutableDict properties()        {return _properties;}

    uint64_t contentLength() const {
        Value len = _properties[kCBLBlobLengthProperty];
        if (len.isInteger())
            return len.asUnsigned();
        else
            return c4blob_getSize(store(), _key);
    }

    const char* digest() const {
        if (_digest.empty()) {
            alloc_slice digest(c4blob_keyToString(_key));
            const_cast<CBLBlob*>(this)->_digest = string(digest);
        }
        return _digest.c_str();
    }

    const char* contentType() const {
        slice type = _properties[kCBLBlobContentTypeProperty].asString();
        if (!type)
            return nullptr;
        if (type != slice(_contentTypeCache))
            const_cast<CBLBlob*>(this)->_contentTypeCache = string(type);
        return _contentTypeCache.c_str();
    }

    void setContentType(const char *type) {
        if (type)
            _properties[kCBLBlobContentTypeProperty] = type;
        else
            _properties[kCBLBlobContentTypeProperty].remove();
    }

    virtual FLSliceResult getContents(C4Error *outError) const {
        return c4blob_getContents(store(), _key, outError);
    }

    virtual bool openStream(C4Error *outError) const {
        close();
        const_cast<CBLBlob*>(this)->_reader = c4blob_openReadStream(store(), _key, outError);
        return (_reader != nullptr);
    }

    size_t read(void *dst, size_t maxLength, C4Error *outError) const {
        if (!_reader) {
            setError(outError, LiteCoreDomain, kC4ErrorNotOpen, "Blob is not open for reading"_sl);
            return -1;
        }
        return c4stream_read(_reader, dst, maxLength, outError);
    }

    void close() const {
        if (_reader) {
            c4stream_close(_reader);
            const_cast<CBLBlob*>(this)->_reader = nullptr;
        }
    }

    virtual bool install(CBLDatabase *db _cbl_nonnull, C4Error *outError) {
        return true;
    }

protected:
    // constructor for MutableBlob to call
    CBLBlob(const char *contentType)
    :_properties(MutableDict::newDict())
    {
        _properties[kCBLTypeProperty] = kCBLBlobType;
        if (contentType)
            _properties[kCBLBlobContentTypeProperty] = contentType;
    }

    ~CBLBlob()                                          {close();}

    CBLDatabase* database() const                       {return _db;}
    void setDatabase(CBLDatabase *db)                   {_db = db;}

    C4BlobStore* store() const {
        assert(_db);
        return _db->blobStore();
    }

    const C4BlobKey& key() const                        {return _key;}

    void setKey(const C4BlobKey &key, uint64_t length) {
        _key = key;
        _properties[kCBLBlobDigestProperty] = digest();
        _properties[kCBLBlobLengthProperty] = length;
    }

private:
    bool findDatabase() {
        assert(!_db);
        const CBLDocument* doc = CBLDocument::containing(_properties);
        if (doc)
            _db = doc->database();
        return (_db != nullptr);
    }

    CBLDatabase*            _db {nullptr};
    C4BlobKey               _key {};
    string                  _digest;
    string                  _contentTypeCache;
    MutableDict             _properties;
    C4ReadStream*           _reader {nullptr};
};


class CBLNewBlob : public CBLBlob {
public:
    // Constructor for new blobs, given contents or writer (but not both)
    CBLNewBlob(const char *contentType,
               slice contents,
               C4WriteStream *writer)
    :CBLBlob(contentType)
    ,_contents(contents)
    ,_writer(writer)
    {
        C4BlobKey key;
        uint64_t length;
        if (contents) {
            assert(!writer);
            key = c4blob_computeKey(contents);
            length = contents.size;
        } else {
            assert(writer);
            key = c4stream_computeBlobKey(writer);
            length = c4stream_bytesWritten(writer);
        }
        setKey(key, length);
        CBLDocument::registerNewBlob(this);
    }

    virtual FLSliceResult getContents(C4Error *outError) const override {
        if (database()) {
            return CBLBlob::getContents(outError);
        } else if (_contents) {
            return FLSliceResult(const_cast<alloc_slice&>(_contents));
        } else {
            setError(outError, LiteCoreDomain, kC4ErrorNotFound,
                     "Can't get streamed blob contents until doc is saved"_sl);
            return FLSliceResult{};
        }
    }

    virtual bool openStream(C4Error *outError) const override {
        if (database()) {
            return CBLBlob::openStream(outError);
        } else {
            setError(outError, LiteCoreDomain, kC4ErrorNotFound,
                     "Can't stream blob until doc is saved"_sl);
            return false;
        }
    }

    virtual bool install(CBLDatabase *db _cbl_nonnull, C4Error *outError) override {
        cbl_log(kCBLLogDomainDatabase, CBLLogInfo, "Saving new blob '%s'", digest());
        assert(database() == nullptr || database() == db);
        const C4BlobKey &expectedKey = key();
        if (_contents) {
            if (!c4blob_create(db->blobStore(), _contents, &expectedKey, nullptr, outError))
                return false;
            _contents = nullslice;
        } else {
            assert(_writer);
            if (!c4stream_install(_writer, &expectedKey, outError))
                return false;
            c4stream_closeWriter(_writer);
            _writer = nullptr;
        }
        setDatabase(db);
        CBLDocument::unregisterNewBlob(this);
        return true;
    }

protected:
    ~CBLNewBlob() {
        c4stream_closeWriter(_writer);
        if (!database())
            CBLDocument::unregisterNewBlob(this);
    }

private:
    alloc_slice _contents;              // Contents, before save
    C4WriteStream* _writer {nullptr};   // Stream, before save
};
