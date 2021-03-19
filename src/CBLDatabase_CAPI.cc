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


CBLDatabaseConfiguration CBLDatabaseConfiguration_Default() CBLAPI {
    return CBLDatabase::defaultConfiguration(); //FIXME: Catch?
}


bool CBL_DatabaseExists(FLString name, FLString inDirectory) CBLAPI {
    return CBLDatabase::exists(name, inDirectory);//FIXME: Catch
}


bool CBL_CopyDatabase(FLString fromPath,
                      FLString toName,
                      const CBLDatabaseConfiguration* config,
                      CBLError* outError) CBLAPI
{
    CBLDatabase::copyDatabase(fromPath, toName, config);//FIXME: Catch
    return true;
}


bool CBL_DeleteDatabase(FLString name,
                        FLString inDirectory,
                        CBLError *outError) CBLAPI
{
    CBLDatabase::deleteDatabase(name, inDirectory);  //FIXME: Catch exceptions
    return true;
}

CBLDatabase* CBLDatabase_Open(FLString name,
                              const CBLDatabaseConfiguration *config,
                              CBLError *outError) CBLAPI
{
    return CBLDatabase::open(name, config).detach(); //FIXME: Catch
}


bool CBLDatabase_Close(CBLDatabase* db, CBLError* outError) CBLAPI {
    if (db)
        db->close();  //FIXME: Catch exceptions
    return true;
}

bool CBLDatabase_BeginTransaction(CBLDatabase* db, CBLError* outError) CBLAPI {
    db->beginTransaction();  //FIXME: Catch exceptions
    return true;
}

bool CBLDatabase_EndTransaction(CBLDatabase* db, bool commit, CBLError* outError) CBLAPI {
    db->endTransaction(commit);  //FIXME: Catch exceptions
    return true;
}

bool CBLDatabase_Delete(CBLDatabase* db, CBLError* outError) CBLAPI {
    db->closeAndDelete();  //FIXME: Catch exceptions
    return true;
}

#ifdef COUCHBASE_ENTERPRISE
bool CBLDatabase_ChangeEncryptionKey(CBLDatabase *db,
                                     const CBLEncryptionKey *newKey,
                                     CBLError* outError) CBLAPI
{
    db->changeEncryptionKey(newKey);//FIXME: Catch
    return true;
}
#endif

bool CBLDatabase_PerformMaintenance(CBLDatabase* db,
                                    CBLMaintenanceType type,
                                    CBLError* outError) CBLAPI
{
    db->performMaintenance(type);//FIXME: Catch
    return true;
}


FLString CBLDatabase_Name(const CBLDatabase* db) CBLAPI {
    return db->name();
}

FLStringResult CBLDatabase_Path(const CBLDatabase* db) CBLAPI {
    return FLStringResult(db->path());//FIXME: Catch
}

const CBLDatabaseConfiguration CBLDatabase_Config(const CBLDatabase* db) CBLAPI {
    return db->config();
}

uint64_t CBLDatabase_Count(const CBLDatabase* db) CBLAPI {
    return db->count();//FIXME: Catch
}

uint64_t CBLDatabase_LastSequence(const CBLDatabase* db) CBLAPI {
    return db->lastSequence();//FIXME: Catch
}


#pragma mark - DOCUMENTS:


const CBLDocument* CBLDatabase_GetDocument(const CBLDatabase* db, FLString docID,
                                           CBLError* outError) CBLAPI
{
    auto doc = db->getDocument(docID, false).detach();  //FIXME: Catch
    if (!doc && outError)
        outError->code = 0;
    return doc;
}


CBLDocument* CBLDatabase_GetMutableDocument(CBLDatabase* db, FLString docID,
                                            CBLError* outError) CBLAPI
{
    auto doc = db->getMutableDocument(docID).detach();  //FIXME: Catch
    if (!doc && outError)
        outError->code = 0;
    return doc;
}


bool CBLDatabase_SaveDocument(CBLDatabase* db,
                              CBLDocument* doc,
                              CBLError* outError) CBLAPI
{
    return CBLDatabase_SaveDocumentWithConcurrencyControl(db, doc,
                                                          kCBLConcurrencyControlLastWriteWins,
                                                          outError);
}

bool CBLDatabase_SaveDocumentWithConcurrencyControl(CBLDatabase* db,
                                                    CBLDocument* doc,
                                                    CBLConcurrencyControl concurrency,
                                                    CBLError* outError) CBLAPI
{
    if (doc->save(db, {concurrency}))//FIXME: Catch
        return true;
    C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
    return false;
}

bool CBLDatabase_SaveDocumentWithConflictHandler(CBLDatabase* db _cbl_nonnull,
                                                 CBLDocument* doc _cbl_nonnull,
                                                 CBLConflictHandler conflictHandler,
                                                 void *context,
                                                 CBLError* outError) CBLAPI
{
    if (doc->save(db, {conflictHandler, context}))//FIXME: Catch
        return true;
    C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
    return false;
}

bool CBLDatabase_DeleteDocument(CBLDatabase *db _cbl_nonnull,
                                const CBLDocument* doc _cbl_nonnull,
                                CBLError* outError) CBLAPI
{
    return CBLDatabase_DeleteDocumentWithConcurrencyControl(db, doc,
                                                            kCBLConcurrencyControlLastWriteWins,
                                                            outError);
}

bool CBLDatabase_DeleteDocumentWithConcurrencyControl(CBLDatabase *db _cbl_nonnull,
                                                      const CBLDocument* doc _cbl_nonnull,
                                                      CBLConcurrencyControl concurrency,
                                                      CBLError* outError) CBLAPI
{
    try {
        if (const_cast<CBLDocument*>(doc)->deleteDoc(db, concurrency))
            return true;
        C4Error::set(LiteCoreDomain, kC4ErrorConflict, {}, internal(outError));
        return false;
    } catch (...) {
        C4Error::fromCurrentException(internal(outError));
        return false;
    }
}

bool CBLDatabase_DeleteDocumentByID(CBLDatabase* db _cbl_nonnull,
                                    FLString docID,
                                    CBLError* outError) CBLAPI
{
    if (CBLDocument::deleteDoc(db, docID))//FIXME: Catch
        return true;
    C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
    return false;

}

bool CBLDatabase_PurgeDocument(CBLDatabase* db _cbl_nonnull,
                               const CBLDocument* doc _cbl_nonnull,
                               CBLError* outError) CBLAPI
{
    return CBLDatabase_PurgeDocumentByID(doc->database(), doc->docID(), outError);
}

bool CBLDatabase_PurgeDocumentByID(CBLDatabase* db,
                                   FLString docID,
                                   CBLError* outError) CBLAPI
{
    if (db->purgeDocument(docID))//FIXME: Catch
        return true;
    C4Error::set(LiteCoreDomain, kC4ErrorNotFound, {}, internal(outError));
    return false;
}

CBLTimestamp CBLDatabase_GetDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                               FLSlice docID,
                                               CBLError* error) CBLAPI
{
    return db->getDocumentExpiration(docID);//FIXME: Catch
}

bool CBLDatabase_SetDocumentExpiration(CBLDatabase* db _cbl_nonnull,
                                       FLSlice docID,
                                       CBLTimestamp expiration,
                                       CBLError* error) CBLAPI
{
    db->setDocumentExpiration(docID, expiration); //FIXME: Catch
    return true;
}


#pragma mark - QUERIES:


bool CBLDatabase_CreateIndex(CBLDatabase *db _cbl_nonnull,
                             FLString name,
                             CBLIndexSpec spec,
                             CBLError *outError) CBLAPI
{
    db->createIndex(name, spec);  //FIXME: Catch
    return true;
}


bool CBLDatabase_DeleteIndex(CBLDatabase *db _cbl_nonnull,
                             FLString name,
                             CBLError *outError) CBLAPI
{
    db->deleteIndex(name);  //FIXME: Catch
    return true;
}


FLMutableArray CBLDatabase_IndexNames(CBLDatabase *db _cbl_nonnull) CBLAPI {
    return db->indexNames();  //FIXME: Catch
}


CBLListenerToken* CBLDatabase_AddChangeListener(const CBLDatabase* constdb,
                                                CBLDatabaseChangeListener listener,
                                                void *context) CBLAPI
{
    return const_cast<CBLDatabase*>(constdb)->addListener(listener, context).detach();
}


CBLListenerToken* CBLDatabase_AddChangeDetailListener(const CBLDatabase* constdb,
                                                      CBLDatabaseChangeDetailListener listener,
                                                      void *context) CBLAPI
{
    return const_cast<CBLDatabase*>(constdb)->addListener(listener, context).detach();
}


void CBLDatabase_BufferNotifications(CBLDatabase *db,
                                     CBLNotificationsReadyCallback callback,
                                     void *context) CBLAPI
{
    db->bufferNotifications(callback, context);
}

void CBLDatabase_SendNotifications(CBLDatabase *db) CBLAPI {
    db->sendNotifications();
}


CBLListenerToken* CBLDatabase_AddDocumentChangeListener(const CBLDatabase* db _cbl_nonnull,
                                                        FLString docID,
                                                        CBLDocumentChangeListener listener _cbl_nonnull,
                                                        void *context) CBLAPI
{
    return const_cast<CBLDatabase*>(db)->addDocListener(docID, listener, context).detach();
}
