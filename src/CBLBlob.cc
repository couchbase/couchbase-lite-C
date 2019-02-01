//
// CBLBlob.cc
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
#include "Internal.hh"
#include "Util.hh"
#include "c4BlobStore.h"
#include "c4Document+Fleece.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"

using namespace std;
using namespace fleece;


const FLSlice kCBLTypeProperty              = FLSTR(kC4ObjectTypeProperty);
const FLSlice kCBLBlobType                  = FLSTR(kC4ObjectType_Blob);
const FLSlice kCBLBlobDigestProperty        = FLSTR(kC4BlobDigestProperty);
const FLSlice kCBLBlobLengthProperty        = "length"_sl;
const FLSlice kCBLBlobContentTypeProperty   = "content_type"_sl;


static inline C4WriteStream* internal(CBLBlobWriteStream *writer) {return (C4WriteStream*)writer;}


class CBLBlob : public CBLRefCounted {
public:
    // Constructor for existing blobs
    CBLBlob(CBLDatabase *db _cbl_nonnull, Dict properties) {
        slice digest = properties[kCBLBlobDigestProperty].asString();
        if (cbl_isBlob(properties) && c4blob_keyFromString(digest, &_key)) {
            _db = db;
            _properties = properties.asMutable();
            if (!_properties)
                _properties = properties.mutableCopy();
        }
    }

    // Constructor for new blobs, given either contents or a writer (not both)
    CBLBlob(CBLDatabase *db _cbl_nonnull,
            const char *contentType,
            slice contents,
            C4WriteStream *writer,
            C4Error *outError)
    {
        uint64_t length;
        bool ok;
        if (contents) {
            assert(!writer);
            length = contents.size;
            ok = c4blob_create(db->blobStore(), contents, nullptr, &_key, outError);
        } else {
            assert(writer);
            length = c4stream_bytesWritten(writer);
            _key = c4stream_computeBlobKey(writer);
            ok = c4stream_install(writer, nullptr, outError);
        }
        if (!ok)
            return;

        _db = db;
        _properties = MutableDict::newDict();
        _properties[kCBLBlobDigestProperty] = digest();
        _properties[kCBLBlobLengthProperty] = length;
        if (contentType)
            _properties[kCBLBlobContentTypeProperty] = contentType;
    }

    CBLBlob* retainIfValid()        {return _db ? retain(this) : nullptr;}

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
        if (!_contentType) {
            slice type = _properties[kCBLBlobContentTypeProperty].asString();
            const_cast<CBLBlob*>(this)->_contentType.reset(new string(type));
        }
        return _contentType->c_str();
    }

    void setContentType(const char *type) {
        _contentType.reset();
        if (type)
            _properties[kCBLBlobContentTypeProperty] = type;
        else
            _properties[kCBLBlobContentTypeProperty].remove();
    }

    FLSliceResult getContents(C4Error *outError) const {
        return c4blob_getContents(store(), _key, outError);
    }

    bool openStream(C4Error *outError) const {
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

private:
    ~CBLBlob()                      {close();}
    C4BlobStore* store() const      {return _db->blobStore();}

    CBLDatabase*    _db {nullptr};
    C4BlobKey       _key {};
    string          _digest;
    unique_ptr<string> _contentType;
    MutableDict     _properties;
    C4ReadStream*   _reader {nullptr};
};


#pragma mark - C API:


bool cbl_isBlob(FLDict dict) CBLAPI {
    C4BlobKey digest;
    return dict && c4doc_dictIsBlob(dict, &digest);
}

const CBLBlob* cbl_db_getBlob(CBLDatabase *db, FLDict blobDict) CBLAPI {
    auto blob = retained(new CBLBlob(db, blobDict));
    return blob->retainIfValid();
}

CBLBlob* cbl_db_getMutableBlob(CBLDatabase *db, FLMutableDict blobDict) CBLAPI {
    return const_cast<CBLBlob*>(cbl_db_getBlob(db, blobDict));
}

FLDict cbl_blob_properties(const CBLBlob* blob) CBLAPI {
    return blob->properties();
}

FLMutableDict cbl_blob_mutableProperties(CBLBlob* blob) CBLAPI {
    return blob->properties();
}

const char* cbl_blob_contentType(const CBLBlob* blob) CBLAPI {
    return blob->contentType();
}

void cbl_blob_setContentType(CBLBlob* blob, const char* contentType) CBLAPI {
    blob->setContentType(contentType);
}

uint64_t cbl_blob_length(const CBLBlob* blob) CBLAPI {
    return blob->contentLength();
}

const char* cbl_blob_digest(const CBLBlob* blob) CBLAPI {
    return blob->digest();
}

CBLBlobContents cbl_blob_getContents(const CBLBlob* blob, CBLError *outError) CBLAPI {
    FLSliceResult c = blob->getContents(internal(outError));
    return {c.buf, c.size};
}

void cbl_blob_freeContents(const CBLBlob* _cbl_nonnull, CBLBlobContents c) CBLAPI {
    FLSliceResult_Free({c.data, c.length});
}

bool cbl_blob_openContentStream(const CBLBlob* blob, CBLError *outError) CBLAPI {
    return blob->openStream(internal(outError));
}

ssize_t cbl_blob_readContent(const CBLBlob* blob,
                             void *dst,
                             size_t maxLength,
                             CBLError *outError) CBLAPI
{
    return blob->read(dst, maxLength, internal(outError));
}

void cbl_blob_closeContentStream(const CBLBlob* blob) CBLAPI {
    blob->close();
}


#pragma mark - CREATING BLOBS:


static CBLBlob* createBlob(CBLDatabase *db,
                           const char *type,
                           FLSlice contents,
                           CBLBlobWriteStream *writer,
                           CBLError *outError)
{
    auto blob = retained(new CBLBlob(db, type, contents, internal(writer), internal(outError)));
    return blob->retainIfValid();
}

CBLBlob* cbl_db_createBlobWithData(CBLDatabase *db,
                                   const char *contentType,
                                   FLSlice contents,
                                   CBLError *outError) CBLAPI
{
    return createBlob(db, contentType, contents, nullptr, outError);
}

CBLBlob* cbl_db_createBlobWithStream(CBLDatabase *db,
                                     const char *contentType,
                                     CBLBlobWriteStream *writer,
                                     CBLError *outError) CBLAPI
{
    return createBlob(db, contentType, nullslice, writer, outError);
}


CBLBlobWriteStream* cbl_blobwriter_new(CBLDatabase *db, CBLError *outError) CBLAPI {
    return (CBLBlobWriteStream*) c4blob_openWriteStream(db->blobStore(), internal(outError));
}

void cbl_blobwriter_free(CBLBlobWriteStream* writer) CBLAPI {
    c4stream_closeWriter(internal(writer));
}

bool cbl_blobwriter_write(CBLBlobWriteStream* writer,
                          const void *data,
                          size_t length,
                          CBLError *outError) CBLAPI
{
    return c4stream_write(internal(writer), data, length, internal(outError));
}
