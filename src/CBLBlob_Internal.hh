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
#include "Internal.hh"
#include "c4BlobStore.hh"
#include "c4Document.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <mutex>


struct CBLBlob : public CBLRefCounted {
public:
    static bool isBlob(FLDict dict) noexcept {
        return C4Blob::isBlob(dict);
    }

    static const CBLBlob* getBlob(FLDict blobDict) noexcept {
        auto doc = CBLDocument::containing(Dict(blobDict));
        if (!doc)
            return nullptr;
        return doc->getBlob(blobDict);
    }

    Dict properties() const                                 {return _properties.asDict();}

    virtual alloc_slice content() const                     {return blobStore()->getContents(_key);}

    std::unique_ptr<CBLBlobReadStream> openContentStream() const;

    alloc_slice toJSON() const {
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
    friend class CBLDocument;

    // Constructor for existing blobs -- called by CBLDocument::getBlob()
    CBLBlob(CBLDocument *doc, Dict properties, const C4BlobKey &key)
    :_db(doc->database())
    ,_key(key)
    ,_properties(properties)
    {
        assert_precondition(_db);
        assert_precondition(properties);
    }

    // constructor for subclass CBLNewBlob to call.
    explicit CBLBlob(const C4BlobKey &key, uint64_t length, slice contentType)
    :_key(key)
    ,_properties(MutableDict::newDict())
    {
        auto mp = properties().asMutable();
        assert(mp != nullptr);
        mp[kCBLTypeProperty] = kCBLBlobType;
        mp[kCBLBlobDigestProperty] = C4Blob::keyToString(_key);
        mp[kCBLBlobLengthProperty] = length;
        if (contentType)
            mp[kCBLBlobContentTypeProperty] = contentType;
    }

    const C4BlobKey& key() const                        {return _key;}
    CBLDatabase* database() const                       {return _db;}
    void setDatabase(CBLDatabase *db _cbl_nonnull)      {precondition(!_db); _db = db;}

    C4BlobStore* blobStore() const {
        if (!_db) C4Error::raise(LiteCoreDomain, kC4ErrorNotFound, "Unsaved blob");
        return _db->blobStore();
    }

private:
    RetainedValue const _properties;
    C4BlobKey           _key;
    CBLDatabase*        _db {nullptr};
};



struct CBLNewBlob : public CBLBlob {
public:
    CBLNewBlob(slice contentType, slice contents)
    :CBLBlob(C4Blob::computeKey(contents), contents.size, contentType)
    {
        precondition(contents);
        _content = contents;
        CBLDocument::registerNewBlob(this);
    }

    CBLNewBlob(slice contentType, CBLBlobWriteStream &&writer);

    virtual alloc_slice content() const override {
        {
            LOCK(_mutex);
            if (_content)
                    return _content;
        }
        return CBLBlob::content();
    }

    bool install(CBLDatabase *db _cbl_nonnull) {
        {
            LOCK(_mutex);
            CBL_Log(kCBLLogDomainDatabase, CBLLogInfo, "Saving new blob '%.*s'", FMTSLICE(digest()));
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
                return true;
            }
            setDatabase(db);
        }
        CBLDocument::unregisterNewBlob(this);
        return true;
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
    virtual size_t read(void *buffer, size_t maxBytes) =0;
    virtual int64_t getLength() const =0;
    virtual void seek(int64_t pos) =0;
    virtual ~CBLBlobReadStream();
protected:
    CBLBlobReadStream() = default;
};



struct CBLBlobWriteStream {
    static std::unique_ptr<CBLBlobWriteStream> create(CBLDatabase*);
    virtual void write(slice) =0;
    virtual ~CBLBlobWriteStream();
protected:
    friend class CBLNewBlob;
    CBLBlobWriteStream() = default;
};
