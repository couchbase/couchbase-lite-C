//
// CBLBlob.h
//
// Copyright (c) 2018 Couchbase, Inc All rights reserved.
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
#include "CBLBase.h"
#include "fleece/Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup blobs Blobs
    @{
    A CBLBlob is a binary data blob associated with a document.
 */

    extern const FLSlice kCBLTypeProperty;
    extern const FLSlice kCBLBlobType;

    extern const FLSlice kCBLBlobDigestProperty;
    extern const FLSlice kCBLBlobLengthProperty;
    extern const FLSlice kCBLBlobContentTypeProperty;


    /** Returns true if a dictionary in a document is a blob reference.
        If so, you can call \ref cbl_db_getBlob or \ref cbl_db_getMutableBlob to access it. */
    bool cbl_isBlob(FLDict) CBLAPI;

    
    CBL_REFCOUNTED(CBLBlob*, blob);

    /** Instantiates a CBLBlob object corresponding to a blob dictionary in a document.
        @param db  The database containing this dictionary.
        @param blobDict  A dictionary in a document.
        @return  A CBLBlob instance for this blob, or NULL if the dictionary is not a blob. */
    const CBLBlob* cbl_db_getBlob(CBLDatabase *db _cbl_nonnull,
                                  FLDict blobDict) CBLAPI;

    /** Instantiates a mutable CBLBlob object corresponding to a blob in a mutable document.
        Changes made to the blob's properties will be reflected in the dictionary.
        @param db  The database containing this dictionary.
        @param blobDict  A dictionary in a document.
        @return  A CBLBlob instance for this blob, or NULL if the dictionary is not a blob. */
    CBLBlob* cbl_db_getMutableBlob(CBLDatabase *db _cbl_nonnull,
                                   FLMutableDict blobDict _cbl_nonnull) CBLAPI;


#pragma mark - BLOB METADATA:

    /** Returns the length in bytes of a blob's content. */
    uint64_t cbl_blob_length(const CBLBlob* _cbl_nonnull) CBLAPI;

    /** Returns the cryptographic digest of a blob's content. */
    const char* cbl_blob_digest(const CBLBlob* _cbl_nonnull) CBLAPI;

    /** Returns a blob's MIME type, if its metadata has a kCBLBlobContentTypeProperty. */
    const char* cbl_blob_contentType(const CBLBlob* _cbl_nonnull) CBLAPI;

    /** Sets the MIME type of a mutable blob (its kCBLBlobContentTypeProperty). */
    void cbl_blob_setContentType(CBLBlob* _cbl_nonnull, const char* contentType) CBLAPI;

    /** Returns a blob's metadata. This includes the `digest`, `length` and `content_type`
        properties, as well as any custom ones that may have been added. */
    FLDict cbl_blob_properties(const CBLBlob* _cbl_nonnull) CBLAPI;

    /** Returns a mutable blob's properties in mutable form.
        @warning  Do not alter the `digest` or `length` properties! */
    FLMutableDict cbl_blob_mutableProperties(CBLBlob* _cbl_nonnull) CBLAPI;


#pragma mark - READING:

    typedef struct {
        const void *data;
        size_t length;
    } CBLBlobContents;

    /** Reads the blob's contents from the database and returns them.
        You are responsible for calling \ref cbl_blob_freeContents on the returned data when done.
        @warning  This can potentially allocate a very large heap block! */
    CBLBlobContents cbl_blob_getContents(const CBLBlob* _cbl_nonnull, CBLError *outError) CBLAPI;

    /** Frees the memory allocated by \ref cbl_blob_getContents. */
    void cbl_blob_freeContents(const CBLBlob* _cbl_nonnull, CBLBlobContents) CBLAPI;

    /** Opens a stream for reading a blob's contents.
        If the stream is already open, resets it to the beginning. */
    bool cbl_blob_openContentStream(const CBLBlob* _cbl_nonnull, CBLError *outError) CBLAPI;

    /** Reads data from a blob. The blob must have been opened via \ref cbl_blob_openContentStream.
        @param blob  The blob to read from.
        @param dst  The address to copy the read data to.
        @param maxLength  The number of bytes to read.
        @param outError  On failure, an error will be stored here if non-NULL.
        @return  The actual number of bytes read; 0 if at EOF, -1 on error. */
    ssize_t cbl_blob_readContent(const CBLBlob* blob _cbl_nonnull,
                       void *dst _cbl_nonnull,
                       size_t maxLength,
                       CBLError *outError) CBLAPI;

    /** Closes a blob's input stream. */
    void cbl_blob_closeContentStream(const CBLBlob* _cbl_nonnull) CBLAPI;


#pragma mark - CREATING:

    /** Creates a new blob given its contents as a single block of data.
        @note  The memory pointed to by `contents` is no longer needed after this call completes
                (it will have been written to the database.)
        @param db  The database the blob will be added to.
        @param contentType  The MIME type (optional).
        @param contents  The data's address and length.
        @param outError  On failure, error info will be written here.
        @return  A new CBLBlob instance, or NULL on error. */
    CBLBlob* cbl_db_createBlobWithData(CBLDatabase *db _cbl_nonnull,
                                       const char *contentType,
                                       FLSlice contents,
                                       CBLError *outError) CBLAPI;

    /** A stream for writing a new blob to the database. */
    typedef struct CBLBlobWriteStream CBLBlobWriteStream;

    /** Opens a stream for writing a new blob.
        You should call \ref cbl_blobwriter_write one or more times to write the data,
        then \ref cbl_db_createBlobWithStream to create the blob, then \ref cbl_blobwriter_free.

        If for some reason you need to abort, just free the writer without calling
        \ref cbl_db_createBlobWithStream. */
    CBLBlobWriteStream* cbl_blobwriter_new(CBLDatabase* _cbl_nonnull,
                                           CBLError *outError) CBLAPI;

    /** Closes a blob-writing stream after you're finished. */
    void cbl_blobwriter_free(CBLBlobWriteStream*) CBLAPI;

    /** Writes data to a blob.
        @param writer  The stream to write to.
        @param data  The address of the data to write.
        @param length  The length of the data to write.
        @param outError  On failure, error info will be written here.
        @return  True on success, false on failure. */
    bool cbl_blobwriter_write(CBLBlobWriteStream* writer _cbl_nonnull,
                              const void *data _cbl_nonnull,
                              size_t length,
                              CBLError *outError) CBLAPI;

    /** Creates a new blob after its data has been written to a \ref CBLBlobWriteStream.
        @note  You still need to free the stream afterwards.
        @param db  The database the blob will be added to.
        @param contentType  The MIME type (optional).
        @param writer  The blob-writing stream the data was written to.
        @param outError  On failure, error info will be written here.
        @return  A new CBLBlob instance, or NULL on error. */
    CBLBlob* cbl_db_createBlobWithStream(CBLDatabase *db _cbl_nonnull,
                                         const char *contentType,
                                         CBLBlobWriteStream* writer _cbl_nonnull,
                                         CBLError *outError) CBLAPI;

#pragma mark - FLEECE UTILITIES:

    /** Returns true if a value in a document is a blob reference.
        If so, you can call \ref FLValue_GetBlob to access it. */
    static inline bool FLValue_IsBlob(FLValue v) {
        return cbl_isBlob(FLValue_AsDict(v));
    }

    /** Instantiates a CBLBlob object corresponding to a blob dictionary in a document.
        @param value  The value (dictionary) in the document.
        @param db  The database containing this dictionary.
        @return  A CBLBlob instance for this blob, or NULL if the value is not a blob.
        @note You are responsible for releasing the CBLBlob object.  */
    static inline const CBLBlob* FLValue_GetBlob(FLValue value, CBLDatabase *db) {
        return cbl_db_getBlob(db, FLValue_AsDict(value));
    }

    /** Stores a blob in a mutable array. */
    static inline void CBLMutableArray_SetBlob(FLMutableArray array _cbl_nonnull,
                                               uint32_t index,
                                               CBLBlob* blob _cbl_nonnull)
    {
        FLMutableArray_SetValue(array, index, (FLValue)cbl_blob_mutableProperties(blob));
    }

    /** Stores a blob in a mutable dictionary. */
    static inline void CBLMutableDict_SetBlob(FLMutableDict dict _cbl_nonnull,
                                FLString key,
                                CBLBlob* blob _cbl_nonnull)
    {
        FLMutableDict_SetValue(dict, key, (FLValue)cbl_blob_mutableProperties(blob));
    }

/** @} */

#ifdef __cplusplus
}
#endif
