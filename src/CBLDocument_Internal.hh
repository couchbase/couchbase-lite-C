//
//  CBLDocument_Internal.hh
//  CBL_C
//
//  Created by Jens Alfke on 1/14/19.
//  Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "CBLDocument.h"
#include "Internal.hh"
#include "c4.hh"
#include "c4Document+Fleece.h"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"

using namespace std;
using namespace fleece;


class CBLDocument : public CBLRefCounted {
public:
    // Construct a new document (not in any database yet)
    CBLDocument(const char *docID, bool isMutable)
    :CBLDocument(ensureDocID(docID), nullptr, nullptr, isMutable)
    { }

    // Construct on an existing document
    CBLDocument(CBLDatabase *db, const string &docID, bool isMutable)
    :CBLDocument(docID, db, c4doc_get(internal(db), slice(docID), true, nullptr), isMutable)
    { }

    // Mutable copy of another CBLDocument
    CBLDocument(const CBLDocument* otherDoc)
    :CBLDocument(otherDoc->_docID,
                 otherDoc->_db,
                 c4doc_retain(otherDoc->_c4doc),
                 true)
    {
        if (otherDoc->isMutable() && otherDoc->_properties)
            _properties = otherDoc->_properties.asDict().mutableCopy(kFLDeepCopyImmutables);
    }

    // Document loaded from db without a C4Document (e.g. a replicator validation callback)
    CBLDocument(CBLDatabase *db,
                const string &docID,
                C4RevisionFlags revFlags,
                Dict body)
    :CBLDocument(docID, db, nullptr, false)
    {
        _properties = body;
    }

    virtual ~CBLDocument()                      { }

    CBLDatabase* database() const               {return _db;}
    const char* docID() const                   {return _docID.c_str();}
    bool exists() const                         {return _c4doc != nullptr;}
    uint64_t sequence() const                   {return _c4doc ? _c4doc->sequence : 0;}
    bool isMutable() const                      {return _mutable;}
    MutableDict mutableProperties() const       {return properties().asMutable();}
    Dict properties() const;

    char* propertiesAsJSON() const;
    bool setPropertiesAsJSON(const char *json, C4Error* outError);

    RetainedConst<CBLDocument> save(CBLDatabase* db _cblnonnull,
                                    bool deleting,
                                    CBLConcurrencyControl concurrency,
                                    C4Error* outError) const;

    bool deleteDoc(CBLConcurrencyControl concurrency,
                   C4Error* outError) const;

    static bool deleteDoc(CBLDatabase* db _cblnonnull,
                          const char* docID _cblnonnull,
                          C4Error* outError);

private:
    // Core constructor
    CBLDocument(const string &docID,
                CBLDatabase *db,
                C4Document *d,          // must be a +1 ref
                bool isMutable)
    :_docID(docID)
    ,_db(db)
    ,_c4doc(d)
    ,_mutable(isMutable)
    { }

    void initProperties();

    static string ensureDocID(const char *docID);

    string const                _docID;                 // Document ID (never empty)
    Retained<CBLDatabase> const _db;                    // Database (null for new doc)
    c4::ref<C4Document> const   _c4doc;                 // LiteCore doc (null for new doc)
    RetainedValue               _properties;            // Properties, initialized lazily
    bool const                  _mutable {false};       // True iff I am mutable
};
