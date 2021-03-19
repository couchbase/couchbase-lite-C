//
// CBLBlob_CAPI.cc
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
#include "c4BlobStore.hh"

using namespace std;
using namespace fleece;


CBL_CORE_API const FLSlice kCBLTypeProperty              = C4Blob::kObjectTypeProperty;
CBL_CORE_API const FLSlice kCBLBlobType                  = C4Blob::kObjectType_Blob;
CBL_CORE_API const FLSlice kCBLBlobDigestProperty        = C4Blob::kDigestProperty;
CBL_CORE_API const FLSlice kCBLBlobLengthProperty        = C4Blob::kLengthProperty;
CBL_CORE_API const FLSlice kCBLBlobContentTypeProperty   = C4Blob::kContentTypeProperty;


bool FLDict_IsBlob(FLDict dict) CBLAPI {
    C4BlobKey digest;
    return dict && C4Blob::isBlob(dict, digest);
}

const CBLBlob* FLDict_GetBlob(FLDict blobDict) CBLAPI {
    auto doc = CBLDocument::containing(Dict(blobDict));
    if (!doc)
        return nullptr;
    return doc->getBlob(blobDict);
}

FLDict CBLBlob_Properties(const CBLBlob* blob) CBLAPI {
    return blob->properties();
}

FLStringResult CBLBlob_ToJSON(const CBLBlob* blob _cbl_nonnull) CBLAPI {
    return FLStringResult(blob->propertiesAsJSON());
}

FLString CBLBlob_ContentType(const CBLBlob* blob) CBLAPI {
    return blob->contentType();
}

uint64_t CBLBlob_Length(const CBLBlob* blob) CBLAPI {
    return blob->contentLength();
}

FLString CBLBlob_Digest(const CBLBlob* blob) CBLAPI {
    return blob->digest();
}

FLSliceResult CBLBlob_Content(const CBLBlob* blob, CBLError *outError) CBLAPI {
    return FLSliceResult(blob->getContents());
}

CBLBlobReadStream* CBLBlob_OpenContentStream(const CBLBlob* blob, CBLError *outError) CBLAPI {
    return external(blob->openStream().release());
}

int CBLBlobReader_Read(CBLBlobReadStream* stream,
                       void *dst,
                       size_t maxLength,
                       CBLError *outError) CBLAPI
{
    return (int) internal(stream)->read(dst, maxLength);
}

void CBLBlobReader_Close(CBLBlobReadStream* stream) CBLAPI {
    delete internal(stream);
}


#pragma mark - CREATING BLOBS:


CBLBlob* CBLBlob_NewWithData(FLString contentType,
                             FLSlice contents) CBLAPI
{
    return retain(new CBLNewBlob(contentType, contents));
}

CBLBlob* CBLBlob_NewWithStream(FLString contentType,
                               CBLBlobWriteStream* writer) CBLAPI
{
    return retain(new CBLNewBlob(contentType, *internal(writer)));
}

CBLBlobWriteStream* CBLBlobWriter_New(CBLDatabase *db, CBLError *outError) CBLAPI {
    return external(new C4WriteStream(*db->blobStore()));
}

void CBLBlobWriter_Close(CBLBlobWriteStream* writer) CBLAPI {
    delete internal(writer);
}

bool CBLBlobWriter_Write(CBLBlobWriteStream* writer,
                         const void *data,
                         size_t length,
                         CBLError *outError) CBLAPI
{
    internal(writer)->write({data, length});
    return true;
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
