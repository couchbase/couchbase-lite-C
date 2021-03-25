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

using namespace std;
using namespace fleece;


CBL_CORE_API const FLSlice kCBLTypeProperty              = C4Blob::kObjectTypeProperty;
CBL_CORE_API const FLSlice kCBLBlobType                  = C4Blob::kObjectType_Blob;
CBL_CORE_API const FLSlice kCBLBlobDigestProperty        = C4Blob::kDigestProperty;
CBL_CORE_API const FLSlice kCBLBlobLengthProperty        = C4Blob::kLengthProperty;
CBL_CORE_API const FLSlice kCBLBlobContentTypeProperty   = C4Blob::kContentTypeProperty;


FLDict CBLBlob_Properties(const CBLBlob* blob) noexcept {
    return blob->properties();
}

FLStringResult CBLBlob_ToJSON(const CBLBlob* blob) noexcept {
    return FLStringResult(blob->toJSON());
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
        return FLSliceResult(blob->content());
    } catchAndBridge(outError)
}

CBLBlobReadStream* CBLBlob_OpenContentStream(const CBLBlob* blob, CBLError *outError) noexcept {
    try {
        return blob->openContentStream().release();
    } catchAndBridge(outError)
}

int CBLBlobReader_Read(CBLBlobReadStream* stream,
                       void *dst,
                       size_t maxLength,
                       CBLError *outError) noexcept
{
    try {
        return (int) stream->read(dst, maxLength);
    } catchAndBridgeReturning(outError, -1)
}

void CBLBlobReader_Close(CBLBlobReadStream* stream) noexcept {
    delete stream;
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
        return retain(new CBLNewBlob(contentType, move(*writer)));
    } catchAndWarn()
}

CBLBlobWriteStream* CBLBlobWriter_New(CBLDatabase *db, CBLError *outError) noexcept {
    try {
        return new CBLBlobWriteStream(db);
    } catchAndBridge(outError)
}

void CBLBlobWriter_Close(CBLBlobWriteStream* writer) noexcept {
    delete writer;
}

bool CBLBlobWriter_Write(CBLBlobWriteStream* writer,
                         const void *data,
                         size_t length,
                         CBLError *outError) noexcept
{
    try {
        writer->write({data, length});
        return true;
    } catchAndBridge(outError)
}


#pragma mark - FLEECE UTILITIES:


bool FLDict_IsBlob(FLDict dict) noexcept {
    return CBLBlob::isBlob(dict);
}

const CBLBlob* FLDict_GetBlob(FLDict blobDict) noexcept {
    return CBLBlob::getBlob(blobDict);
}

void FLSlot_SetBlob(FLSlot slot, CBLBlob* blob) noexcept
{
    Dict props = blob->properties();
    MutableDict mProps = props.asMutable();
    if (!mProps)
        mProps = props.mutableCopy();
    FLSlot_SetValue(slot, mProps);
}
