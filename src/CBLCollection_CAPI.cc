//
// CBLCollection_CAPI.cc
//
// Copyright (C) 2022 Couchbase, Inc All rights reserved.
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

#include "CBLCollection.h"
#include "CBLCollection_Internal.hh"
#include "CBLDatabase_Internal.hh"

using namespace fleece;

#pragma mark - CONSTANTS

const FLString kCBLDefaultCollectionName = FLSTR("_default");

#pragma mark - SCOPE AND COLLECTION MANAGEMENT

FLMutableArray CBLDatabase_ScopeNames(const CBLDatabase* db, CBLError* outError) noexcept {
    try {
        return db->scopeNames();
    } catchAndBridge(outError)
}

FLMutableArray CBLDatabase_CollectionNames(const CBLDatabase* db,
                                           FLString scopeName,
                                           CBLError* outError) noexcept
{
    try {
        return db->collectionNames(scopeName);
    } catchAndBridge(outError)
}

CBLScope* CBLDatabase_Scope(const CBLDatabase* db, FLString scopeName, CBLError* outError) noexcept {
    try {
        return const_cast<CBLDatabase*>(db)->getScope(scopeName).detach();
    } catchAndBridge(outError)
}

CBLCollection* CBLDatabase_Collection(const CBLDatabase* db,
                                      FLString collectionName,
                                      FLString scopeName,
                                      CBLError* outError) noexcept
{
    try {
        return const_cast<CBLDatabase*>(db)->getCollection(collectionName, scopeName).detach();
    } catchAndBridge(outError)
}

CBLCollection* CBLDatabase_CreateCollection(CBLDatabase* db,
                                            FLString collectionName,
                                            FLString scopeName,
                                            CBLError* outError) noexcept
{
    try {
        return db->createCollection(collectionName, scopeName).detach();
    } catchAndBridge(outError)
}

bool CBLDatabase_DeleteCollection(CBLDatabase* db,
                                  FLString collectionName,
                                  FLString scopeName,
                                  CBLError* outError) noexcept
{
    try {
        return db->deleteCollection(collectionName, scopeName);
    } catchAndBridge(outError)
}

CBLScope* CBLDatabase_DefaultScope(const CBLDatabase* db, CBLError* outError) noexcept {
    try {
        return const_cast<CBLDatabase*>(db)->getDefaultScope().detach();
    } catchAndBridge(outError)
}

CBLCollection* CBLDatabase_DefaultCollection(const CBLDatabase* db, CBLError* outError) noexcept {
    try {
        return const_cast<CBLDatabase*>(db)->getDefaultCollection(false).detach();
    } catchAndBridge(outError)
}

#pragma mark - ACCESSORS

CBLScope* CBLCollection_Scope(const CBLCollection* collection) noexcept {
    try {
        return const_cast<CBLCollection*>(collection)->scope().detach();
    } catchAndWarn()
}

/** Returns the collection name. */
FLString CBLCollection_Name(const CBLCollection* collection) noexcept {
    try {
        return collection->name();
    } catchAndWarn()
}

/** Returns the number of documents in the collection. */
uint64_t CBLCollection_Count(const CBLCollection* collection) noexcept {
    try {
        return collection->count();
    } catchAndWarn()
}

/** Private API */
CBLDatabase* CBLCollection_Database(const CBLCollection* collection) noexcept {
    try {
        return collection->database();
    } catchAndWarn()
}

/** Private API */
uint64_t CBLCollection_LastSequence(const CBLCollection* collection) noexcept {
    try {
        return collection->lastSequence();
    } catchAndWarn()
}

#pragma mark - DOCUMENTS

const CBLDocument* CBLCollection_GetDocument(const CBLCollection* collection, FLString docID,
                                             CBLError* outError) noexcept
{
    try {
        auto doc = collection->getDocument(docID).detach();
        if (!doc && outError)
            outError->code = 0;
        return doc;
    } catchAndBridge(outError)
}

CBLDocument* CBLCollection_GetMutableDocument(CBLCollection* collection, FLString docID,
                                              CBLError* outError) noexcept
{
    try {
        auto doc = collection->getMutableDocument(docID).detach();
        if (!doc && outError)
            outError->code = 0;
        return doc;
    } catchAndBridge(outError)
}

bool CBLCollection_SaveDocument(CBLCollection* collection,
                                CBLDocument* doc,
                                CBLError* outError,
                                uint32_t maxRevTreeDepth = 0) noexcept
{
    return CBLCollection_SaveDocumentWithConcurrencyControl(collection, doc,
                                                            kCBLConcurrencyControlLastWriteWins,
                                                            outError, maxRevTreeDepth);
}

bool CBLCollection_SaveDocumentWithConcurrencyControl(CBLCollection* collection,
                                                      CBLDocument* doc,
                                                      CBLConcurrencyControl concurrency,
                                                      CBLError* outError,
                                                      uint32_t maxRevTreeDepth = 0) noexcept
{
    try {
        if (doc->save(collection, {concurrency}, maxRevTreeDepth))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_SaveDocumentWithConflictHandler(CBLCollection* collection,
                                                   CBLDocument* doc,
                                                   CBLConflictHandler conflictHandler,
                                                   void *context,
                                                   CBLError* outError,
                                                   uint32_t maxRevTreeDepth = 0) noexcept
{
    try {
        if (doc->save(collection, {conflictHandler, context}, maxRevTreeDepth))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_DeleteDocument(CBLCollection *collection,
                                  const CBLDocument* doc,
                                  CBLError* outError) noexcept
{
    return CBLCollection_DeleteDocumentWithConcurrencyControl(collection, doc,
                                                              kCBLConcurrencyControlLastWriteWins,
                                                              outError);
}

bool CBLCollection_DeleteDocumentWithConcurrencyControl(CBLCollection *collection,
                                                        const CBLDocument* doc,
                                                        CBLConcurrencyControl concurrency,
                                                        CBLError* outError) noexcept
{
    try {
        if (collection->deleteDocument(doc, concurrency))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_DeleteDocumentByID(CBLCollection* collection,
                                      FLString docID,
                                      CBLError* _cbl_nullable outError) noexcept
{
    try {
        if (collection->deleteDocument(docID))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLCollection_PurgeDocument(CBLCollection* collection,
                                 const CBLDocument* doc,
                                 CBLError* outError) noexcept
{
    try {
        CBLDocument::checkCollectionMatches(doc->collection(), collection);
        return CBLCollection_PurgeDocumentByID(collection, doc->docID(), outError);
    } catchAndBridge(outError)
}

bool CBLCollection_PurgeDocumentByID(CBLCollection* collection,
                                     FLString docID,
                                     CBLError* outError) noexcept
{
    try {
        if (collection->purgeDocument(docID))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

CBLTimestamp CBLCollection_GetDocumentExpiration(CBLCollection* collection,
                                                 FLSlice docID,
                                                 CBLError* outError) noexcept
{
    try {
        return collection->getDocumentExpiration(docID);
    } catchAndBridgeReturning(outError, -1)
}

bool CBLCollection_SetDocumentExpiration(CBLCollection* collection,
                                         FLSlice docID,
                                         CBLTimestamp expiration,
                                         CBLError* outError) noexcept
{
    try {
        collection->setDocumentExpiration(docID, expiration);
        return true;
    } catchAndBridge(outError)
}

#pragma mark - INDEXES:

bool CBLCollection_CreateValueIndex(CBLCollection *collection,
                                    FLString name,
                                    CBLValueIndexConfiguration config,
                                    CBLError *outError) noexcept
{
    try {
        collection->createValueIndex(name, config);
        return true;
    } catchAndBridge(outError)
}

bool CBLCollection_CreateFullTextIndex(CBLCollection *collection,
                                       FLString name,
                                       CBLFullTextIndexConfiguration config,
                                       CBLError *outError) noexcept
{
    try {
        collection->createFullTextIndex(name, config);
        return true;
    } catchAndBridge(outError)
}

bool CBLCollection_DeleteIndex(CBLCollection *collection,
                               FLString name,
                               CBLError *outError) noexcept
{
    try {
        collection->deleteIndex(name);
        return true;
    } catchAndBridge(outError)
}

FLMutableArray CBLCollection_GetIndexNames(CBLCollection *collection, CBLError *outError) noexcept {
    try {
        return FLMutableArray_Retain(collection->indexNames());
    } catchAndBridge(outError)
}

#pragma mark - LISTENERS:

CBLListenerToken* CBLCollection_AddChangeListener(const CBLCollection* collection,
                                                  CBLCollectionChangeListener listener,
                                                  void* _cbl_nullable context) noexcept
{
    try {
        // NOTE: In case there is an exception the function will log and return a dummy token.
        return const_cast<CBLCollection*>(collection)->addChangeListener(listener, context).detach();
    } catchAndBridgeReturning(nullptr, make_retained<CBLListenerToken>((const void*)listener, nullptr).detach())
}

CBLListenerToken* CBLCollection_AddDocumentChangeListener(const CBLCollection* collection,
                                                        FLString docID,
                                                        CBLCollectionDocumentChangeListener listener,
                                                        void *context) noexcept
{
    try {
        // NOTE: In case there is an exception the function will log and return a dummy token.
        return const_cast<CBLCollection*>(collection)->addDocumentListener(docID, listener, context).detach();
    } catchAndBridgeReturning(nullptr, make_retained<CBLListenerToken>((const void*)listener, nullptr).detach())
}
