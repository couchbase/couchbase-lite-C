//
// CBLDatabase_CAPI.cc
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

#include "CBLDatabase_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "CBLDatabase.h"

using namespace std;
using namespace fleece;
using namespace cbl_internal;


CBLDatabaseConfiguration CBLDatabaseConfiguration_Default() noexcept {
    return CBLDatabase::defaultConfiguration();
}


bool CBL_DatabaseExists(FLString name, FLString inDirectory) noexcept {
    try {
        return CBLDatabase::exists(name, inDirectory);
    } catchAndWarn();
}


bool CBL_CopyDatabase(FLString fromPath,
                      FLString toName,
                      const CBLDatabaseConfiguration* config,
                      CBLError* outError) noexcept
{
    try {
        CBLDatabase::copyDatabase(fromPath, toName, config);
        return true;
    } catchAndBridge(outError)
}


bool CBL_DeleteDatabase(FLString name,
                        FLString inDirectory,
                        CBLError *outError) noexcept
{
    try {
        CBLDatabase::deleteDatabase(name, inDirectory);
        return true;
    } catchAndBridge(outError)
}

CBLDatabase* CBLDatabase_Open(FLString name,
                              const CBLDatabaseConfiguration *config,
                              CBLError *outError) noexcept
{
    try {
        return CBLDatabase::open(name, config).detach();
    } catchAndBridge(outError)
}


bool CBLDatabase_Close(CBLDatabase* db, CBLError* outError) noexcept {
    try {
        if (db)
            db->close();
        return true;
    } catchAndBridge(outError)
}

bool CBLDatabase_BeginTransaction(CBLDatabase* db, CBLError* outError) noexcept {
    try {
        db->beginTransaction();
        return true;
    } catchAndBridge(outError)
}

bool CBLDatabase_EndTransaction(CBLDatabase* db, bool commit, CBLError* outError) noexcept {
    try {
        db->endTransaction(commit);
        return true;
    } catchAndBridge(outError)
}

bool CBLDatabase_Delete(CBLDatabase* db, CBLError* outError) noexcept {
    try {
        db->closeAndDelete();
        return true;
    } catchAndBridge(outError)
}

#ifdef COUCHBASE_ENTERPRISE
bool CBLDatabase_ChangeEncryptionKey(CBLDatabase *db,
                                     const CBLEncryptionKey *newKey,
                                     CBLError* outError) noexcept
{
    try {
        db->changeEncryptionKey(newKey);
        return true;
    } catchAndBridge(outError)
}
#endif

bool CBLDatabase_PerformMaintenance(CBLDatabase* db,
                                    CBLMaintenanceType type,
                                    CBLError* outError) noexcept
{
    try {
        db->performMaintenance(type);
        return true;
    } catchAndBridge(outError)
}


FLString CBLDatabase_Name(const CBLDatabase* db) noexcept {
    return db->name();
}

FLStringResult CBLDatabase_Path(const CBLDatabase* db) noexcept {
    return FLStringResult(db->path());
}

const CBLDatabaseConfiguration CBLDatabase_Config(const CBLDatabase* db) noexcept {
    return db->config();
}

uint64_t CBLDatabase_Count(const CBLDatabase* db) noexcept {
    try {
        return db->count();
    } catchAndWarn();
}

uint64_t CBLDatabase_LastSequence(const CBLDatabase* db) noexcept {
    try {
        return db->lastSequence();
    } catchAndWarn()
}


#pragma mark - DOCUMENTS:


const CBLDocument* CBLDatabase_GetDocument(const CBLDatabase* db, FLString docID,
                                           CBLError* outError) noexcept
{
    try {
        auto doc = db->getDocument(docID).detach();
        if (!doc && outError)
            outError->code = 0;
        return doc;
    } catchAndBridge(outError)
}


CBLDocument* CBLDatabase_GetMutableDocument(CBLDatabase* db, FLString docID,
                                            CBLError* outError) noexcept
{
    try {
        auto doc = db->getMutableDocument(docID).detach();
        if (!doc && outError)
            outError->code = 0;
        return doc;
    } catchAndBridge(outError)
}


bool CBLDatabase_SaveDocument(CBLDatabase* db,
                              CBLDocument* doc,
                              CBLError* outError) noexcept
{
    return CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc,
                                                          kCBLConcurrencyControlLastWriteWins,
                                                          outError);
}

bool CBLDatabase_SaveDocumentWithConcurrencyControl(CBLDatabase* db,
                                                    CBLDocument* doc,
                                                    CBLConcurrencyControl concurrency,
                                                    CBLError* outError) noexcept
{
    try {
        if (doc->save(db, {concurrency}))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLDatabase_SaveDocumentWithConflictHandler(CBLDatabase* db,
                                                 CBLDocument* doc,
                                                 CBLConflictHandler conflictHandler,
                                                 void *context,
                                                 CBLError* outError) noexcept
{
    try {
        if (doc->save(db, {conflictHandler, context}))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLDatabase_DeleteDocument(CBLDatabase *db,
                                const CBLDocument* doc,
                                CBLError* outError) noexcept
{
    return CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc,
                                                            kCBLConcurrencyControlLastWriteWins,
                                                            outError);
}

bool CBLDatabase_DeleteDocumentWithConcurrencyControl(CBLDatabase *db,
                                                      const CBLDocument* doc,
                                                      CBLConcurrencyControl concurrency,
                                                      CBLError* outError) noexcept
{
    try {
        if (db->deleteDocument(doc, concurrency))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLDatabase_DeleteDocumentByID(CBLDatabase* db,
                                    FLString docID,
                                    CBLError* outError) noexcept
{
    try {
        if (db->deleteDocument(docID))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

bool CBLDatabase_PurgeDocument(CBLDatabase* db,
                               const CBLDocument* doc,
                               CBLError* outError) noexcept
{
    return CBLDatabase_PurgeDocumentByID(doc->database(), doc->docID(), outError);
}

bool CBLDatabase_PurgeDocumentByID(CBLDatabase* db,
                                   FLString docID,
                                   CBLError* outError) noexcept
{
    try {
        if (db->purgeDocument(docID))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
        return false;
    } catchAndBridge(outError)
}

CBLTimestamp CBLDatabase_GetDocumentExpiration(CBLDatabase* db,
                                               FLSlice docID,
                                               CBLError* outError) noexcept
{
    try {
        return db->getDocumentExpiration(docID);
    } catchAndBridge(outError)
}

bool CBLDatabase_SetDocumentExpiration(CBLDatabase* db,
                                       FLSlice docID,
                                       CBLTimestamp expiration,
                                       CBLError* outError) noexcept
{
    try {
        db->setDocumentExpiration(docID, expiration);
        return true;
    } catchAndBridge(outError)
}


#pragma mark - QUERIES:


bool CBLDatabase_CreateValueIndex(CBLDatabase *db,
                                  FLString name,
                                  CBLValueIndexConfiguration config,
                                  CBLError *outError) noexcept
{
    try {
        db->createValueIndex(name, config);
        return true;
    } catchAndBridge(outError)
}

bool CBLDatabase_CreateFullTextIndex(CBLDatabase *db,
                                     FLString name,
                                     CBLFullTextIndexConfiguration config,
                                     CBLError *outError) noexcept
{
    try {
        db->createFullTextIndex(name, config);
        return true;
    } catchAndBridge(outError)
}


bool CBLDatabase_DeleteIndex(CBLDatabase *db,
                             FLString name,
                             CBLError *outError) noexcept
{
    try {
        db->deleteIndex(name);
        return true;
    } catchAndBridge(outError)
}


FLMutableArray CBLDatabase_IndexNames(CBLDatabase *db) noexcept {
    try {
        return FLMutableArray_Retain(db->indexNames());
    } catchAndWarn()
}


CBLListenerToken* CBLDatabase_AddChangeListener(const CBLDatabase* constdb,
                                                CBLDatabaseChangeListener listener,
                                                void *context) noexcept
{
    return const_cast<CBLDatabase*>(constdb)->addListener(listener, context).detach();
}


CBLListenerToken* CBLDatabase_AddChangeDetailListener(const CBLDatabase* constdb,
                                                      CBLDatabaseChangeDetailListener listener,
                                                      void *context) noexcept
{
    return const_cast<CBLDatabase*>(constdb)->addListener(listener, context).detach();
}


void CBLDatabase_BufferNotifications(CBLDatabase *db,
                                     CBLNotificationsReadyCallback callback,
                                     void *context) noexcept
{
    db->bufferNotifications(callback, context);
}

void CBLDatabase_SendNotifications(CBLDatabase *db) noexcept {
    db->sendNotifications();
}


CBLListenerToken* CBLDatabase_AddDocumentChangeListener(const CBLDatabase* db,
                                                        FLString docID,
                                                        CBLDocumentChangeListener listener,
                                                        void *context) noexcept
{
    return const_cast<CBLDatabase*>(db)->addDocListener(docID, listener, context).detach();
}
