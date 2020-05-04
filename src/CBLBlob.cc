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


CBL_CORE_API const FLSlice kCBLTypeProperty              = FLSTR(kC4ObjectTypeProperty);
CBL_CORE_API const FLSlice kCBLBlobType                  = FLSTR(kC4ObjectType_Blob);
CBL_CORE_API const FLSlice kCBLBlobDigestProperty        = FLSTR(kC4BlobDigestProperty);
CBL_CORE_API const FLSlice kCBLBlobLengthProperty        = "length"_sl;
CBL_CORE_API const FLSlice kCBLBlobContentTypeProperty   = "content_type"_sl;


bool CBL_IsBlob(FLDict dict) CBLAPI {
    C4BlobKey digest;
    return dict && c4doc_dictIsBlob(dict, &digest);
}

const CBLBlob* CBLBlob_Get(FLDict blobDict) CBLAPI {
    auto doc = CBLDocument::containing(Dict(blobDict));
    if (!doc)
        return nullptr;
    return doc->getBlob(blobDict);
}

FLDict CBLBlob_Properties(const CBLBlob* blob) CBLAPI {
    return blob->properties();
}

const char* CBLBlob_ContentType(const CBLBlob* blob) CBLAPI {
    return blob->contentType();
}

uint64_t CBLBlob_Length(const CBLBlob* blob) CBLAPI {
    return blob->contentLength();
}

const char* CBLBlob_Digest(const CBLBlob* blob) CBLAPI {
    return blob->digest();
}

FLSliceResult CBLBlob_LoadContent(const CBLBlob* blob, CBLError *outError) CBLAPI {
    return blob->getContents(internal(outError));
}

CBLBlobReadStream* CBLBlob_OpenContentStream(const CBLBlob* blob, CBLError *outError) CBLAPI {
    return (CBLBlobReadStream*)blob->openStream(internal(outError));
}

int CBLBlobReader_Read(CBLBlobReadStream* stream,
                            void *dst,
                            size_t maxLength,
                            CBLError *outError) CBLAPI
{
    return (int) c4stream_read(internal(stream), dst, maxLength, internal(outError));
}

void CBLBlobReader_Close(CBLBlobReadStream* stream) CBLAPI {
    c4stream_close(internal(stream));
}


#pragma mark - CREATING BLOBS:


static CBLBlob* createNewBlob(slice contentType,
                              FLSlice contents,
                              CBLBlobWriteStream *writer)
{
    return retain(new CBLNewBlob(contentType, contents, internal(writer)));
}

CBLBlob* CBLBlob_CreateWithData(const char *contentType,
                                FLSlice contents) CBLAPI
{
    return createNewBlob(contentType, contents, nullptr);
}

CBLBlob* CBLBlob_CreateWithData_s(FLString contentType,
                                  FLSlice contents) CBLAPI
{
    return createNewBlob(contentType, contents, nullptr);
}

CBLBlob* CBLBlob_CreateWithStream(const char *contentType,
                                  CBLBlobWriteStream *writer) CBLAPI
{
    return createNewBlob(contentType, nullslice, writer);
}

CBLBlob* CBLBlob_CreateWithStream_s(FLString contentType,
                                    CBLBlobWriteStream* writer) CBLAPI
{
    return createNewBlob(contentType, nullslice, writer);
}

CBLBlobWriteStream* CBLBlobWriter_New(CBLDatabase *db, CBLError *outError) CBLAPI {
    return (CBLBlobWriteStream*) c4blob_openWriteStream(db->blobStore(),
                                                        internal(outError));
}

void CBLBlobWriter_Close(CBLBlobWriteStream* writer) CBLAPI {
    c4stream_closeWriter(internal(writer));
}

bool CBLBlobWriter_Write(CBLBlobWriteStream* writer,
                          const void *data,
                          size_t length,
                          CBLError *outError) CBLAPI
{
    return c4stream_write(internal(writer), data, length, internal(outError));
}


#pragma mark - FLEECE UTILITIES:


void FLSlot_SetBlob(FLSlot slot _cbl_nonnull, CBLBlob* blob _cbl_nonnull) CBLAPI
{
    Dict props = CBLBlob_Properties(blob);
    MutableDict mProps = props.asMutable();
    if (!mProps)
        mProps = props.mutableCopy();
    FLSlot_SetValue(slot, mProps);
}


void FLMutableArray_SetBlob(FLMutableArray array _cbl_nonnull,
                             uint32_t index,
                             CBLBlob* blob _cbl_nonnull) CBLAPI //deprecated
{
    FLSlot_SetBlob(FLMutableArray_Set(array, index), blob);
}


void FLMutableDict_SetBlob(FLMutableDict dict _cbl_nonnull,
                                          FLString key,
                                          CBLBlob* blob _cbl_nonnull) CBLAPI //deprecated
{
    FLSlot_SetBlob(FLMutableDict_Set(dict, key), blob);
}

