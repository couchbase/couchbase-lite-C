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

#include "CBLBlob_Internal.hh"

using namespace std;
using namespace fleece;


const FLSlice kCBLTypeProperty              = FLSTR(kC4ObjectTypeProperty);
const FLSlice kCBLBlobType                  = FLSTR(kC4ObjectType_Blob);
const FLSlice kCBLBlobDigestProperty        = FLSTR(kC4BlobDigestProperty);
const FLSlice kCBLBlobLengthProperty        = "length"_sl;
const FLSlice kCBLBlobContentTypeProperty   = "content_type"_sl;


bool cbl_isBlob(FLDict dict) CBLAPI {
    C4BlobKey digest;
    return dict && c4doc_dictIsBlob(dict, &digest);
}

const CBLBlob* cbl_blob_get(FLDict blobDict) CBLAPI {
    auto doc = CBLDocument::containing(Dict(blobDict));
    if (!doc) {
        C4Warn("cbl_doc_getBlob: Dict at %p does not belong to any CBLDocument", blobDict);
        return nullptr;
    }
    return doc->getBlob(blobDict);
}

CBLBlob* cbl_blob_getMutable(FLMutableDict blobDict) CBLAPI {
    return const_cast<CBLBlob*>(cbl_blob_get(blobDict));
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


static CBLBlob* createNewBlob(const char *type,
                              FLSlice contents,
                              CBLBlobWriteStream *writer)
{
    return retain(new CBLNewBlob(type, contents, internal(writer)));
}

CBLBlob* cbl_doc_createBlobWithData(const char *contentType,
                                    FLSlice contents) CBLAPI
{
    return createNewBlob(contentType, contents, nullptr);
}

CBLBlob* cbl_doc_createBlobWithStream(const char *contentType,
                                      CBLBlobWriteStream *writer) CBLAPI
{
    return createNewBlob(contentType, nullslice, writer);
}


CBLBlobWriteStream* cbl_blobwriter_new(CBLDatabase *db, CBLError *outError) CBLAPI {
    return (CBLBlobWriteStream*) c4blob_openWriteStream(db->blobStore(),
                                                        internal(outError));
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
