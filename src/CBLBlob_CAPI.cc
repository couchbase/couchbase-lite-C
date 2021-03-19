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
#include "Util.hh"

using namespace std;
using namespace fleece;


CBL_CORE_API const FLSlice kCBLTypeProperty              = C4Blob::kObjectTypeProperty;
CBL_CORE_API const FLSlice kCBLBlobType                  = C4Blob::kObjectType_Blob;
CBL_CORE_API const FLSlice kCBLBlobDigestProperty        = C4Blob::kDigestProperty;
CBL_CORE_API const FLSlice kCBLBlobLengthProperty        = C4Blob::kLengthProperty;
CBL_CORE_API const FLSlice kCBLBlobContentTypeProperty   = C4Blob::kContentTypeProperty;


bool FLDict_IsBlob(FLDict dict) noexcept {
    C4BlobKey digest;
    return dict && C4Blob::isBlob(dict, digest);
}

const CBLBlob* FLDict_GetBlob(FLDict blobDict) noexcept {
    auto doc = CBLDocument::containing(Dict(blobDict));
    if (!doc)
        return nullptr;
    return doc->getBlob(blobDict);
}

FLDict CBLBlob_Properties(const CBLBlob* blob) noexcept {
    return blob->properties();
}

FLStringResult CBLBlob_ToJSON(const CBLBlob* blob _cbl_nonnull) noexcept {
    return FLStringResult(blob->propertiesAsJSON());
}

FLString CBLBlob_ContentType(const CBLBlob* blob) noexcept {
    return blob->contentType();
}

uint64_t CBLBlob_Length(const CBLBlob* blob) noexcept {
    return blob->contentLength();
}

FLString CBLBlob_Digest(const CBLBlob* blob) noexcept {
    return blob->digest();
}

FLSliceResult CBLBlob_Content(const CBLBlob* blob, CBLError *outError) noexcept {
    try {
        return FLSliceResult(blob->getContents());
    } catchAndBridge(outError)
}

CBLBlobReadStream* CBLBlob_OpenContentStream(const CBLBlob* blob, CBLError *outError) noexcept {
    try {
        return external(blob->openStream().release());
    } catchAndBridge(outError)
}

int CBLBlobReader_Read(CBLBlobReadStream* stream,
                       void *dst,
                       size_t maxLength,
                       CBLError *outError) noexcept
{
    try {
        return (int) internal(stream)->read(dst, maxLength);
    } catchAndBridgeReturning(outError, -1)
}

void CBLBlobReader_Close(CBLBlobReadStream* stream) noexcept {
    delete internal(stream);
}


#pragma mark - CREATING BLOBS:


CBLBlob* CBLBlob_NewWithData(FLString contentType,
                             FLSlice contents) noexcept
{
    try {
        return retain(new CBLNewBlob(contentType, contents));
    } catchAndWarn()
}

CBLBlob* CBLBlob_NewWithStream(FLString contentType,
                               CBLBlobWriteStream* writer) noexcept
{
    try {
        return retain(new CBLNewBlob(contentType, *internal(writer)));
    } catchAndWarn()
}

CBLBlobWriteStream* CBLBlobWriter_New(CBLDatabase *db, CBLError *outError) noexcept {
    try {
        return external(new C4WriteStream(*db->blobStore()));
    } catchAndBridge(outError)
}

void CBLBlobWriter_Close(CBLBlobWriteStream* writer) noexcept {
    delete internal(writer);
}

bool CBLBlobWriter_Write(CBLBlobWriteStream* writer,
                         const void *data,
                         size_t length,
                         CBLError *outError) noexcept
{
    try {
        internal(writer)->write({data, length});
        return true;
    } catchAndBridge(outError)
}


#pragma mark - FLEECE UTILITIES:


void FLSlot_SetBlob(FLSlot slot _cbl_nonnull, CBLBlob* blob _cbl_nonnull) noexcept
{
    Dict props = CBLBlob_Properties(blob);
    MutableDict mProps = props.asMutable();
    if (!mProps)
        mProps = props.mutableCopy();
    FLSlot_SetValue(slot, mProps);
}
