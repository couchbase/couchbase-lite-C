//
//  CBLDocument_Internal.hh
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
#include "CBLDocument.h"
#include "Internal.hh"
#include "CBLDatabase_Internal.hh"
#include "c4.hh"
#include "c4Document+Fleece.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <mutex>
#include <unordered_map>

struct CBLBlob;
struct CBLNewBlob;

struct CBLDocument final : public CBLRefCounted {
    using RetainedConstDocument = fleece::RetainedConst<CBLDocument>;

public:
    // Construct a new document (not in any database yet)
    CBLDocument(slice docID, bool isMutable);

    // Mutable copy of another CBLDocument
    CBLDocument(const CBLDocument* otherDoc);

    // Document loaded from db without a C4Document (e.g. a replicator validation callback)
    CBLDocument(CBLDatabase *db _cbl_nonnull,
                slice docID,
                slice revID,
                C4RevisionFlags revFlags,
                Dict body);

    static CBLDocument* containing(Value value) {
        C4Document* doc = c4doc_containingValue(value);
        return doc ? (CBLDocument*)doc->extraInfo().pointer : nullptr;
    }

    CBLDatabase* database() const               {return _db;}
    slice docID() const                         {return _docID;}
    slice revisionID() const                    {return _revID;}
    alloc_slice canonicalRevisionID() const;
    C4RevisionFlags revisionFlags() const;
    bool exists() const                         {return _c4doc != nullptr;}
    uint64_t sequence() const                   {return _c4doc ? _c4doc->sequence() : 0;}
    bool isMutable() const                      {return _mutable;}


    //---- Properties:

    FLDoc createFleeceDoc() const;
    Dict properties() const;
    MutableDict mutableProperties()             {return properties().asMutable();}
    void setProperties(MutableDict d)           {if (checkMutable(nullptr)) _properties = d;}

    alloc_slice propertiesAsJSON() const;
    bool setPropertiesAsJSON(slice json, C4Error* outError);

    //---- Save/delete:

    struct SaveOptions {
        SaveOptions(CBLConcurrencyControl c)         :concurrency(c) { }
        SaveOptions(CBLConflictHandler h, void *ctx) :conflictHandler(h), context(ctx) { }

        CBLConcurrencyControl concurrency;
        CBLConflictHandler conflictHandler = nullptr;
        void *context;
        bool deleting = false;
    };

    bool save(CBLDatabase* db _cbl_nonnull,
              const SaveOptions&,
              C4Error* outError);

    bool deleteDoc(CBLDatabase* _cbl_nonnull,
                   CBLConcurrencyControl,
                   C4Error* outError);

    static bool deleteDoc(CBLDatabase* db _cbl_nonnull,
                          slice docID,
                          C4Error* outError);

    //---- Blobs:

    CBLBlob* getBlob(FLDict _cbl_nonnull);

    static void registerNewBlob(CBLNewBlob* _cbl_nonnull);
    static void unregisterNewBlob(CBLNewBlob* _cbl_nonnull);

    //---- Conflict resolution:

    // Select a specific revision. Only works if constructed with allRevisions=true.
    bool selectRevision(slice revID);

    // Select a conflicting revision. Only works if constructed with allRevisions=true.
    bool selectNextConflictingRevision();

    enum class Resolution {
        useLocal,
        useRemote,
        useMerge
    };

    bool resolveConflict(Resolution, const CBLDocument *mergeDoc, CBLError*);

// private by convention
    CBLDocument(slice docID, CBLDatabase *db, C4Document *d, bool isMutable);
private:
    virtual ~CBLDocument();

    bool checkMutable(C4Error *outError) const;

    static CBLNewBlob* findNewBlob(FLDict dict _cbl_nonnull);
    bool saveBlobs(CBLDatabase *db, bool &outHasBlobs, C4Error *outError) const;
    alloc_slice encodeBody(CBLDatabase* _cbl_nonnull, C4Database* _cbl_nonnull,
                           C4RevisionFlags &outRevFlags, C4Error *outError) const;
    using ValueToBlobMap = std::unordered_map<FLDict, Retained<CBLBlob>>;
    using UnretainedValueToBlobMap = std::unordered_map<FLDict, CBLNewBlob*>;
    using RetainedDatabase = Retained<CBLDatabase>;
    using RetainedValue = fleece::RetainedValue;
    using recursive_mutex = std::recursive_mutex;

    static UnretainedValueToBlobMap* sNewBlobs;

    RetainedDatabase            _db;                    // Database (null for new doc)
    alloc_slice const           _docID;                 // Document ID (never empty)
    mutable alloc_slice         _revID;                 // Revision ID
    c4::ref<C4Document>         _c4doc;                 // LiteCore doc (null for new doc)
    Doc                         _fromJSON;              // Properties read from JSON
    mutable RetainedValue       _properties;            // Properties, initialized lazily
    ValueToBlobMap              _blobs;                 // Maps Dicts in _properties to CBLBlobs
    mutable recursive_mutex     _mutex;                 // For accessing _c4doc, _properties, _blobs
    bool const                  _mutable {false};       // True iff I am mutable
};
