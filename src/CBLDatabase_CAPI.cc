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
    if (!db)
        return true;
    db->use([=](C4Database *c4db) {
        c4db->close();  //FIXME: Catch exceptions
    });
    return true;
}

bool CBLDatabase_BeginTransaction(CBLDatabase* db, CBLError* outError) CBLAPI {
    db->use([=](C4Database *c4db) {
        c4db->beginTransaction();  //FIXME: Catch exceptions
    });
    return true;
}

bool CBLDatabase_EndTransaction(CBLDatabase* db, bool commit, CBLError* outError) CBLAPI {
    db->use([=](C4Database *c4db) {
        c4db->endTransaction(commit);  //FIXME: Catch exceptions
    });
    return true;
}

bool CBLDatabase_Delete(CBLDatabase* db, CBLError* outError) CBLAPI {
    db->use([=](C4Database *c4db) {
        return c4db->closeAndDeleteFile();  //FIXME: Catch exceptions
    });
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
    return db->performMaintenance(type);//FIXME: Catch
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
