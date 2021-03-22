//
// CBLBlob.cc
//
// Copyright (C) 2020 Jens Alfke. All Rights Reserved.
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
#include "CBLDatabase_Internal.hh"


namespace cbl_internal {
    // These are the concrete subclasses that actually get instantiated.

    struct BlobReadStreamImpl : public CBLBlobReadStream {
        BlobReadStreamImpl(const C4BlobStore &s, C4BlobKey k) :_c4stream(s, k) { }
        ~BlobReadStreamImpl() = default;
        
        size_t read(void *buffer, size_t maxBytes) override {return _c4stream.read(buffer, maxBytes);}
        int64_t getLength() const override                  {return _c4stream.getLength();}
        void seek(int64_t pos) override                     {return _c4stream.seek(pos);}

        C4ReadStream _c4stream;
    };

    struct BlobWriteStreamImpl : public CBLBlobWriteStream {
        BlobWriteStreamImpl(C4BlobStore &s)                 :_c4stream(s) { }
        ~BlobWriteStreamImpl() = default;

        void write(slice data) override                     {return _c4stream.write(data);}

        C4WriteStream _c4stream;
    };

    static inline C4WriteStream& internal(CBLBlobWriteStream &stream) {
        return ((BlobWriteStreamImpl&)stream)._c4stream;
    }
}


CBLBlobReadStream::~CBLBlobReadStream() = default;
CBLBlobWriteStream::~CBLBlobWriteStream() = default;


std::unique_ptr<CBLBlobWriteStream> CBLBlobWriteStream::create(CBLDatabase *db) {
    return std::make_unique<BlobWriteStreamImpl>(*db->blobStore());
}


#pragma mark - CBLBLOB METHODS:


std::unique_ptr<CBLBlobReadStream> CBLBlob::openContentStream() const {
    return std::make_unique<BlobReadStreamImpl>(*blobStore(), _key);
}


CBLNewBlob::CBLNewBlob(slice contentType, CBLBlobWriteStream &&writer)
:CBLBlob(internal(writer).computeBlobKey(), internal(writer).bytesWritten(), contentType) {
    _writer.emplace(std::move(internal(writer)));
    // Nothing more will be written, but don't install the stream until the owning document
    // is saved and calls my install() method.
    CBLDocument::registerNewBlob(this);
}
