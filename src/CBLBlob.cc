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

FLDict cbl_blob_properties(const CBLBlob* blob) CBLAPI {
    return blob->properties();
}

const char* cbl_blob_contentType(const CBLBlob* blob) CBLAPI {
    return blob->contentType();
}

uint64_t cbl_blob_length(const CBLBlob* blob) CBLAPI {
    return blob->contentLength();
}

const char* cbl_blob_digest(const CBLBlob* blob) CBLAPI {
    return blob->digest();
}

FLSliceResult cbl_blob_loadContent(const CBLBlob* blob, CBLError *outError) CBLAPI {
    return blob->getContents(internal(outError));
}

CBLBlobReadStream* cbl_blob_openContentStream(const CBLBlob* blob, CBLError *outError) CBLAPI {
    return (CBLBlobReadStream*)blob->openStream(internal(outError));
}

ssize_t cbl_blobreader_read(CBLBlobReadStream* stream,
                            void *dst,
                            size_t maxLength,
                            CBLError *outError) CBLAPI
{
    return c4stream_read(internal(stream), dst, maxLength, internal(outError));
}

void cbl_blobreader_close(CBLBlobReadStream* stream) CBLAPI {
    c4stream_close(internal(stream));
}


#pragma mark - CREATING BLOBS:


static CBLBlob* createNewBlob(const char *type,
                              FLSlice contents,
                              CBLBlobWriteStream *writer)
{
    return retain(new CBLNewBlob(type, contents, internal(writer)));
}

CBLBlob* cbl_blob_createWithData(const char *contentType,
                                    FLSlice contents) CBLAPI
{
    return createNewBlob(contentType, contents, nullptr);
}

CBLBlob* cbl_blob_createWithStream(const char *contentType,
                                      CBLBlobWriteStream *writer) CBLAPI
{
    return createNewBlob(contentType, nullslice, writer);
}


CBLBlobWriteStream* cbl_blobwriter_new(CBLDatabase *db, CBLError *outError) CBLAPI {
    return (CBLBlobWriteStream*) c4blob_openWriteStream(db->blobStore(),
                                                        internal(outError));
}

void cbl_blobwriter_close(CBLBlobWriteStream* writer) CBLAPI {
    c4stream_closeWriter(internal(writer));
}

bool cbl_blobwriter_write(CBLBlobWriteStream* writer,
                          const void *data,
                          size_t length,
                          CBLError *outError) CBLAPI
{
    return c4stream_write(internal(writer), data, length, internal(outError));
}


#pragma mark - FLEECE UTILITIES:


static MutableDict blobMutableProperties(CBLBlob *blob _cbl_nonnull) {
    Dict props = cbl_blob_properties(blob);
    MutableDict mProps = props.asMutable();
    return mProps ? mProps : props.mutableCopy();
}


void CBLMutableArray_SetBlob(FLMutableArray array _cbl_nonnull,
                             uint32_t index,
                             CBLBlob* blob _cbl_nonnull)
{
    FLMutableArray_SetValue(array, index, blobMutableProperties(blob));
}


void CBLMutableDict_SetBlob(FLMutableDict dict _cbl_nonnull,
                                          FLString key,
                                          CBLBlob* blob _cbl_nonnull)
{
    FLMutableDict_SetValue(dict, key, blobMutableProperties(blob));
}

