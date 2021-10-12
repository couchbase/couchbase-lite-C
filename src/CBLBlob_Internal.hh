//
// CBLBlob_Internal.hh
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

#pragma once
#include "CBLBlob.h"
#include "CBLLog.h"
#include "CBLDatabase_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "CBLQuery_Internal.hh"
#include "Internal.hh"
#include "c4BlobStore.hh"
#include "c4Document.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <mutex>
#include "betterassert.hh"

CBL_ASSUME_NONNULL_BEGIN

struct CBLBlob : public CBLRefCounted {
public:
    static bool isBlob(FLDict _cbl_nullable dict) noexcept {
        return C4Blob::isBlob(dict);
    }

    static inline const CBLBlob* _cbl_nullable getBlob(Dict blobDict) noexcept;

    Dict properties() const                                 {return _properties.asDict();}

    virtual alloc_slice content() const                     {return blobStore()->getContents(_key);}

    virtual void install(CBLDatabase *db) {
        C4Error::raise(LiteCoreDomain, kC4ErrorUnsupported, "No support for re-installing blob getting from database.");
    }
    
    inline std::unique_ptr<CBLBlobReadStream> openContentStream() const;

    alloc_slice createJSON() const {
        if (!_properties)
            return fleece::nullslice;
        fleece::JSONEncoder enc;
        enc.writeValue(_properties);
        return enc.finish();
    }

    uint64_t contentLength() const {
        if (Value len = properties()[kCBLBlobLengthProperty]; len.isInteger())
            return len.asUnsigned();
        else
            return blobStore()->getSize(_key);
    }

    slice digest() const {
        auto digest = properties()[kCBLBlobDigestProperty].asString();
        assert(digest);
        return digest;
    }

    slice contentType() const {
        return properties()[kCBLBlobContentTypeProperty].asString();
    }

protected:
    friend struct CBLDocument;
    friend struct CBLDatabase;
    friend struct CBLResultSet;

    // Constructor for existing blobs -- called by CBLDocument::getBlob()
    CBLBlob(const CBLDatabase *db, Dict properties, const C4BlobKey &key)
    :_db(db)
    ,_key(key)
    ,_properties(properties)
    {
        assert_precondition(_db);
        assert_precondition(properties);
    }
    
    // Constructor for getting existing blobs in the database by blob's dictionary
    CBLBlob(CBLDatabase *db, FLDict properties)
    :_db(db)
    ,_properties(Dict(properties))
    {
        if (!isBlob(properties)) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter,
                           "The properties doesn't contain valid @type key and value.");
        }
        
        auto key = C4Blob::keyFromDigestProperty(properties);
        if (!key) {
            C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter,
                           "The properties doesn't contain digest key.");
        }
        
        if (blobStore()->getSize(*key) < 0) {
            C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Blob doesn't exist in the database.");
        }
        _key = *key;
    }

    // constructor for subclass CBLNewBlob to call.
    explicit CBLBlob(const C4BlobKey &key, uint64_t length, slice contentType)
    :_key(key)
    ,_properties(MutableDict::newDict())
    {
        auto mp = properties().asMutable();
        assert(mp != nullptr);
        mp[kCBLTypeProperty] = kCBLBlobType;
        mp[kCBLBlobDigestProperty] = key.digestString();
        mp[kCBLBlobLengthProperty] = length;
        if (contentType)
            mp[kCBLBlobContentTypeProperty] = contentType;
    }

    const C4BlobKey& key() const                        {return _key;}
    const CBLDatabase* _cbl_nullable database() const   {return _db;}
    void setDatabase(CBLDatabase *db)                   {precondition(!_db); _db = db;}

    C4BlobStore* blobStore() const {
        if (!_db) C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Unsaved blob");
        return _db->blobStore();
    }

private:
    friend struct CBLBlobReadStream;

    fleece::RetainedValue const       _properties;
    C4BlobKey                         _key;
    const CBLDatabase* _cbl_nullable  _db {nullptr};
};



struct CBLNewBlob : public CBLBlob {
public:
    CBLNewBlob(slice contentType, slice contents)
    :CBLBlob(C4BlobKey::computeDigestOfContent(contents), contents.size, contentType)
    {
        precondition(contents);
        _content = contents;
        CBLDocument::registerNewBlob(this);
    }

    inline CBLNewBlob(slice contentType, CBLBlobWriteStream &&writer);

    virtual alloc_slice content() const override {
        {
            LOCK(_mutex);
            if (_content)
                return _content;
        }
        return CBLBlob::content();
    }

    virtual void install(CBLDatabase *db) override {
        {
            LOCK(_mutex);
            CBL_Log(kCBLLogDomainDatabase, kCBLLogInfo, "Saving new blob '%.*s'", FMTSLICE(digest()));
            C4BlobKey expectedKey = key();
            if (_content) {
                db->blobStore()->createBlob(_content, &expectedKey);
                _content = fleece::nullslice;
            } else if (_writer) {
                if (db->blobStore() != &_writer->blobStore())
                    C4Error::raise(LiteCoreDomain, kC4ErrorInvalidParameter,
                                   "Saving blob to wrong database");
                _writer->install(&expectedKey);
                _writer = std::nullopt;
            } else {
                // I'm already installed; this could be a race condition, else a mistake
                if (database() != db) {
                    C4Error::raise(LiteCoreDomain, kC4ErrorUnsupported,
                                   "Trying to save an already-saved blob to a different db");
                }
                return;
            }
            setDatabase(db);
        }
        CBLDocument::unregisterNewBlob(this);
    }

protected:
    ~CBLNewBlob() {
        if (!database())
            CBLDocument::unregisterNewBlob(this);
    }

private:
    mutable std::mutex           _mutex;
    alloc_slice                  _content;  // Blob data, before save
    std::optional<C4WriteStream> _writer ;  // Stream, before save
};


struct CBLBlobReadStream {
    CBLBlobReadStream(const CBLBlob &blob)      :_c4stream(*blob.blobStore(), blob.key()) { }
    size_t read(void *buffer, size_t maxBytes)  {return _c4stream.read(buffer, maxBytes);}
    int64_t getLength() const                   {return _c4stream.getLength();}
    void seek(int64_t pos)                      {return _c4stream.seek(pos);}
private:
    C4ReadStream _c4stream;
};



struct CBLBlobWriteStream {
    CBLBlobWriteStream(CBLDatabase *db)         :_c4stream(*db->blobStore()) { }
    void write(fleece::slice data)              {return _c4stream.write(data);}
private:
    friend struct CBLNewBlob;
    C4WriteStream _c4stream;
};


inline const CBLBlob* _cbl_nullable CBLBlob::getBlob(Dict blobDict) noexcept {
    auto key = C4Blob::keyFromDigestProperty(blobDict);
    if (!key)
        return nullptr;

    // Check if it's a blob or old-style attachment in a saved document:
    if (auto doc = CBLDocument::containing(blobDict); doc)
        return doc->getBlob(blobDict, *key);

    if (!C4Blob::isBlob(blobDict))
        return nullptr;

    // Check if it's a blob in a query result set:
    if (auto rs = CBLResultSet::containing(blobDict); rs)
        return rs->getBlob(blobDict, *key);

    // CBL-2261: Keep this condition last because if this returns null it
    // often logs a warning (so before, every time CBLResultSet::containing
    // was true a benign warning would be logged)
    // Check if it's a new unsaved blob:
    return CBLDocument::findNewBlob(blobDict);
}



inline std::unique_ptr<CBLBlobReadStream> CBLBlob::openContentStream() const {
    return std::make_unique<CBLBlobReadStream>(*this);
}


inline CBLNewBlob::CBLNewBlob(slice contentType, CBLBlobWriteStream &&writer)
:CBLBlob(writer._c4stream.computeBlobKey(), writer._c4stream.getBytesWritten(), contentType) {
    _writer.emplace(std::move(writer._c4stream));
    // Nothing more will be written, but don't install the stream until the owning document
    // is saved and calls my install() method.
    CBLDocument::registerNewBlob(this);
}

CBL_ASSUME_NONNULL_END
