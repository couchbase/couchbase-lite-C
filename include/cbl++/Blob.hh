//
// Blob.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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
#include "Document.hh"
#include "CBLBlob.h"
#include "fleece/Mutable.hh"

namespace cbl {

    /** A reference to a binary data blob associated with a document.
        A blob's persistent form is a special dictionary in the document properties.
        To work with a blob, you construct a Blob object with that dictionary. */
    class Blob : protected RefCounted {
    public:
        static bool isBlob(fleece::Dict d)          {return cbl_isBlob(d);}

        /** Constructs a Blob instance on an existing blob reference in a document. */
        Blob(fleece::Dict d)
        :RefCounted((CBLRefCounted*) cbl_blob_get(d))
        { }

        uint64_t length() const                     {return cbl_blob_length(ref()); }
        const char* contentType() const             {return cbl_blob_contentType(ref()); }
        const char* digest() const                  {return cbl_blob_digest(ref()); }
        fleece::Dict properties() const             {return cbl_blob_properties(ref()); }

        /** A simple class holding the contents of a blob.
            The allocated memory is freed when the object leaves scope. */
        struct Contents : public CBLBlobContents {
            Contents(Contents &&c)
            :CBLBlobContents(c)
            ,_blob(c._blob)
            {c._blob = nullptr;}

            ~Contents() {
                if (_blob)
                    cbl_blob_freeContents(_blob->ref(), *this);
            }

        private:
            friend class Blob;

            Contents(Blob* b, CBLBlobContents c)    :CBLBlobContents(c), _blob(b) { }
            Contents& operator=(const Contents&) =delete;
            Blob *_blob;
        };

        Contents getContents() {
            CBLError error;
            auto c = cbl_blob_getContents(ref(), &error);
            check(c.data, error);
            return Contents(this, c);
        }

        void openContentStream() {
            CBLError error;
            check(cbl_blob_openContentStream(ref(), &error), error);
        }

        size_t readContent(void *dst _cbl_nonnull, size_t maxLength) {
            CBLError error;
            ssize_t read = cbl_blob_readContent(ref(), dst, maxLength, &error);
            check(read >= 0, error);
            return read;
        }

        void closeContentStream() {
            cbl_blob_closeContentStream(ref());
        }

    protected:
        Blob(CBLRefCounted* r)                      :RefCounted(r) { }

        CBL_REFCOUNTED_BOILERPLATE(Blob, RefCounted, const CBLBlob)
    };


    /** A stream for writing a new blob to the database. */
    class BlobWriteStream {
    public:
        BlobWriteStream(Database db) {
            CBLError error;
            _writer = cbl_blobwriter_new(db.ref(), &error);
            if (!_writer) throw error;
        }

        ~BlobWriteStream() {
            cbl_blobwriter_free(_writer);
        }

        void write(fleece::slice data) {
            CBLError error;
            if (!cbl_blobwriter_write(_writer, data.buf, data.size, &error))
                throw error;
        }

    private:
        friend class MutableBlob;
        CBLBlobWriteStream* _writer {nullptr};
    };


    /** A blob in a mutable document, whose properties (except for digest and length) can be
        modified. This subclass is also used to create new blobs. */
    class MutableBlob : public Blob {
    public:
        /** Constructs a Blob instance on an existing blob reference in a mutable document. */
        MutableBlob(fleece::MutableDict dict)
        :Blob(dict)
        { }

        /* Creates a new blob in the database given its contents as a single block of data.
            @note  The memory pointed to by `contents` is no longer needed after this call completes
                    (it will have been written to the database.)
            @param contentType  The MIME type (optional).
            @param contents  The data's address and length. */
        MutableBlob(const char *contentType,
                    fleece::slice contents)
        {
            _ref = (CBLRefCounted*) cbl_doc_createBlobWithData(contentType, contents);
        }

        /** Creates a new blob after its data has been written to a \ref CBLBlobWriteStream.
            @param contentType  The MIME type (optional).
            @param writer  The blob-writing stream the data was written to. */
        MutableBlob(const char *contentType,
                    BlobWriteStream& writer)
        {
            _ref = (CBLRefCounted*) cbl_doc_createBlobWithStream(contentType, writer._writer);
            writer._writer = nullptr;
        }

        fleece::MutableDict properties() {
            return cbl_blob_mutableProperties(ref());
        }

        void setContentType(const char* contentType) {
            cbl_blob_setContentType(ref(), contentType);
        }

        CBL_REFCOUNTED_BOILERPLATE(MutableBlob, Blob, CBLBlob)
    };
}
