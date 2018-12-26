#pragma once
#include "CBLBase.h"
#include "fleece/Fleece.h"

CBL_REFCOUNTED(const CBLDocument*, doc);

typedef CBL_ENUM(uint8_t, CBLConcurrencyControl) {
    kCBLConcurrencyControlLastWriteWins,    ///< The last write operation will win if there is a conflict.
    kCBLConcurrencyControlFailOnConflict    ///< The operation will fail if there is a conflict.
};

const CBLDocument* cbl_db_getDocument(CBLDatabase* _cblnonnull,
                                      const char* _cblnonnull docID);

const CBLDocument* cbl_db_saveDocument(CBLDatabase* _cblnonnull,
                                       CBLDocument* _cblnonnull,
                                       CBLConcurrencyControl,
                                       CBLError*);

bool cbl_db_deleteDocument(CBLDatabase* _cblnonnull,
                           const CBLDocument* _cblnonnull,
                           CBLConcurrencyControl,
                           CBLError*);

bool cbl_db_purgeDocument(CBLDatabase* _cblnonnull,
                          const CBLDocument* _cblnonnull,
                          CBLError*);


const char* cbl_doc_id(const CBLDocument* _cblnonnull);
uint64_t cbl_doc_sequence(const CBLDocument* _cblnonnull);


// Mutable documents

CBLDocument* cbl_doc_new(const char *docID);

CBLDocument* cbl_doc_mutableCopy(const CBLDocument* _cblnonnull);

    // NOTE: C does not have inheritance, so we can't have a CBLMutableDocument that's type-
    // compatible with CBLDocument. Instead, we can make `const CBLDocument*` be an immutable
    // reference, and `CBLDocument*` a mutable one. (This is what CoreFoundation does.)


// Properties

FLDict cbl_doc_properties(const CBLDocument* _cblnonnull);

FLMutableDict cbl_doc_mutableProperties(CBLDocument* _cblnonnull);

    // NOTE: Yes, that's it. The Fleece API has almost everything needed to work with document
    // properties ... the only things missing are dates and blobs.
    // https://github.com/couchbaselabs/fleece/blob/master/API/fleece/Fleece.h
