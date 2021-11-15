//
// CBLBlob+FILE.h
//
// Copyright (c) 2021 Couchbase, Inc All rights reserved.
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

CBL_CAPI_BEGIN

#ifndef _MSC_VER  // sorry, not available on Windows

/** \defgroup blobs Blobs
    @{
    \name  Blob I/O with stdio `FILE`
    @{ */

    /** Opens a stdio `FILE` on a blob's content. You can use this with any read-only stdio
        function that takes a `FILE*`, such as `fread` or `fscanf`.
        @note  You are responsible for calling `fclose` when done with the "file". */
    _cbl_warn_unused
    FILE* _cbl_nullable CBLBlob_OpenAsFILE(CBLBlob* blob,
                                           CBLError* _cbl_nullable) CBLAPI;

    /** Opens a stdio `FILE*` stream for creating a new Blob. You can pass this stream to any
        C library function that writes to a `FILE*`, such as `fwrite` or `fprintf`;
        but you cannot read from nor seek this stream, so `fread` and `fseek` will fail.

        After writing the data, call \ref CBLBlob_CreateWithFILE to create the blob,
        instead of `fclose`.

        If you need to cancel without creating a blob, simply call `fclose` instead. */
    _cbl_warn_unused
    FILE* _cbl_nullable CBLBlobWriter_CreateFILE(CBLDatabase* db,
                                                 CBLError* _cbl_nullable) CBLAPI;

    /** Creates a new blob object from the data written to a `FILE*` stream that was created with
        \ref CBLBlobWriter_CreateFILE.
        You should then add the blob to a mutable document as a property -- see
        \ref FLSlot_SetBlob.
        @note  You are responsible for releasing the CBLBlob reference.
        @note  Do not call `fclose` on the stream; the blob will do that.
        @param contentType  The MIME type (optional).
        @param blobWriter  The stream the data was written to, which must have been created with
                           \ref CBLBlobWriter_CreateFILE.
        @return  A new CBLBlob instance. */
    _cbl_warn_unused
    CBLBlob* CBLBlob_CreateWithFILE(FLString contentType,
                                    FILE* blobWriter) CBLAPI;

/** @} */
/** @} */

CBL_CAPI_END

#endif // _MSC_VER
