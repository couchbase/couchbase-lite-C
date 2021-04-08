//
// CBLCollection_CAPI.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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

#include "CBLCollection_Internal.hh"
#include "CBLDatabase_Internal.hh"
#include "CBLCollection.h"
#include "CBLPrivate.h"

using namespace std;
using namespace fleece;
using namespace cbl_internal;


#pragma mark - CBLDATABASE METHODS:


CBLCollection* CBLDatabase_GetDefaultCollection(CBLDatabase *db) noexcept {
    return db->defaultCollection();
}

bool CBLDatabase_HasCollection(CBLDatabase *db, FLString name) noexcept {
    return db->hasCollection(name);
}

CBLCollection* _cbl_nullable CBLDatabase_GetCollection(CBLDatabase *db, FLString name) noexcept {
    return db->getCollection(name);
}

CBLCollection* CBLDatabase_CreateCollection(CBLDatabase *db,
                                            FLString name,
                                            CBLError* _cbl_nullable outError) noexcept {
    try {
        return db->createCollection(name);
    } catchAndBridge(outError)
}

bool CBLDatabase_DeleteCollection(CBLDatabase *db,
                                  FLString name,
                                  CBLError* _cbl_nullable outError) noexcept {
    try {
        db->deleteCollection(name);
        return true;
    } catchAndBridge(outError)
}

FLMutableArray CBLDatabase_CollectionNames(CBLDatabase *db) noexcept {
    auto array = FLMutableArray_New();
    for (string &name : db->collectionNames())
        FLSlot_SetString(FLMutableArray_Append(array), slice(name));
    return array;
}


#pragma mark - ACCESSORS:


FLString CBLCollection_Name(CBLCollection *coll) noexcept {
    return coll->name();
}

CBLDatabase* CBLCollection_Database(CBLCollection *coll) noexcept {
    return coll->database();
}

uint64_t CBLCollection_Count(CBLCollection *coll) noexcept {
    try {
        return coll->count();
    } catchAndWarn();
}


#pragma mark - DOCUMENTS:


const CBLDocument* CBLCollection_GetDocument(const CBLCollection *coll, FLString docID,
                                           CBLError* outError) noexcept
{
    try {
        auto doc = coll->getDocument(docID).detach();
        if (!doc && outError)
            outError->code = 0;
        return doc;
    } catchAndBridge(outError)
}


CBLDocument* CBLCollection_GetMutableDocument(CBLCollection *coll, FLString docID,
                                            CBLError* outError) noexcept
{
    try {
        auto doc = coll->getMutableDocument(docID).detach();
        if (!doc && outError)
            outError->code = 0;
        return doc;
    } catchAndBridge(outError)
}


bool CBLCollection_SaveDocument(CBLCollection *coll,
                              CBLDocument* doc,
                              CBLError* outError) noexcept
{
    return CBLCollection_SaveDocumentWithConcurrencyControl(coll, doc,
                                                          kCBLConcurrencyControlLastWriteWins,
                                                          outError);
}

bool CBLCollection_SaveDocumentWithConcurrencyControl(CBLCollection *coll,
                                                    CBLDocument* doc,
                                                    CBLConcurrencyControl concurrency,
                                                    CBLError* outError) noexcept
{
    try {
        if (doc->save(coll, {concurrency}))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_SaveDocumentWithConflictHandler(CBLCollection *coll,
                                                 CBLDocument* doc,
                                                 CBLConflictHandler conflictHandler,
                                                 void *context,
                                                 CBLError* outError) noexcept
{
    try {
        if (doc->save(coll, {conflictHandler, context}))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_DeleteDocument(CBLCollection *coll,
                                const CBLDocument* doc,
                                CBLError* outError) noexcept
{
    return CBLCollection_DeleteDocumentWithConcurrencyControl(coll, doc,
                                                            kCBLConcurrencyControlLastWriteWins,
                                                            outError);
}

bool CBLCollection_DeleteDocumentWithConcurrencyControl(CBLCollection *coll,
                                                      const CBLDocument* doc,
                                                      CBLConcurrencyControl concurrency,
                                                      CBLError* outError) noexcept
{
    try {
        if (coll->deleteDocument(doc, concurrency))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_DeleteDocumentByID(CBLCollection *coll,
                                    FLString docID,
                                    CBLError* outError) noexcept
{
    try {
        if (coll->deleteDocument(docID))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_PurgeDocument(CBLCollection *coll,
                               const CBLDocument* doc,
                               CBLError* outError) noexcept
{
    return CBLCollection_PurgeDocumentByID(coll, doc->docID(), outError);
}

bool CBLCollection_PurgeDocumentByID(CBLCollection *coll,
                                   FLString docID,
                                   CBLError* outError) noexcept
{
    try {
        if (coll->purgeDocument(docID))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

CBLTimestamp CBLCollection_GetDocumentExpiration(CBLCollection *coll,
                                               FLSlice docID,
                                               CBLError* outError) noexcept
{
    try {
        return coll->getDocumentExpiration(docID);
    } catchAndBridge(outError)
}

bool CBLCollection_SetDocumentExpiration(CBLCollection *coll,
                                       FLSlice docID,
                                       CBLTimestamp expiration,
                                       CBLError* outError) noexcept
{
    try {
        coll->setDocumentExpiration(docID, expiration);
        return true;
    } catchAndBridge(outError)
}


#pragma mark - QUERIES:


bool CBLCollection_CreateValueIndex(CBLCollection *coll,
                                  FLString name,
                                  CBLValueIndex index,
                                  CBLError *outError) noexcept
{
    try {
        coll->createValueIndex(name, index);
        return true;
    } catchAndBridge(outError)
}

bool CBLCollection_CreateFullTextIndex(CBLCollection *coll,
                                     FLString name,
                                     CBLFullTextIndex index,
                                     CBLError *outError) noexcept
{
    try {
        coll->createFullTextIndex(name, index);
        return true;
    } catchAndBridge(outError)
}


bool CBLCollection_DeleteIndex(CBLCollection *coll,
                             FLString name,
                             CBLError *outError) noexcept
{
    try {
        coll->deleteIndex(name);
        return true;
    } catchAndBridge(outError)
}


FLMutableArray CBLCollection_IndexNames(CBLCollection *coll) noexcept {
    try {
        return FLMutableArray_Retain(coll->indexNames());
    } catchAndWarn()
}
