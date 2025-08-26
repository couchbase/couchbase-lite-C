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
#include "CBLBlob_Internal.hh"
#include "CBLCollection_Internal.hh"
#include "CBLDatabase.h"
#include "CBLDocument_Internal.hh"


using namespace std;
using namespace fleece;
using namespace cbl_internal;


CBLDatabaseConfiguration CBLDatabaseConfiguration_Default() noexcept {
    try {
        return CBLDatabase::defaultConfiguration();
    } catchAndWarn();
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
    try {
        return FLStringResult(db->path());
    } catchAndBridgeReturning(nullptr, FLSliceResult(alloc_slice(kFLSliceNull)));
}

const CBLDatabaseConfiguration CBLDatabase_Config(const CBLDatabase* db) noexcept {
    return db->config();
}

/** Private API */
uint64_t CBLDatabase_LastSequence(const CBLDatabase* db) noexcept {
    try {
        auto col = const_cast<CBLDatabase*>(db)->getInternalDefaultCollection();
        return col->lastSequence();
    } catchAndWarn()
}

/** Private API */
FLSliceResult CBLDatabase_PublicUUID(const CBLDatabase* db) noexcept {
    try {
        return FLSliceResult(db->publicUUID());
    } catchAndWarn()
}


#pragma mark - NOTIFICATION


void CBLDatabase_BufferNotifications(CBLDatabase *db,
                                     CBLNotificationsReadyCallback callback,
                                     void *context) noexcept
{
    db->bufferNotifications(callback, context);
}

void CBLDatabase_SendNotifications(CBLDatabase *db) noexcept {
    db->sendNotifications();
}


#pragma mark - BINDING DEV SUPPORT FOR BLOB:


const CBLBlob* CBLDatabase_GetBlob(CBLDatabase* db, FLDict properties,
                                   CBLError* _cbl_nullable outError) noexcept
{
    try {
        auto blob = db->getBlob(properties).detach();
        if (!blob && outError)
            outError->code = 0;
        return blob;
    } catchAndBridge(outError)
}


bool CBLDatabase_SaveBlob(CBLDatabase* db, CBLBlob* blob,
                          CBLError* _cbl_nullable outError) noexcept
{
    try {
        db->saveBlob(blob);
        return true;
    } catchAndBridge(outError)
}

#pragma mark - EXTENSION:

#ifdef COUCHBASE_ENTERPRISE

bool CBL_EnableVectorSearch(FLString path, CBLError* _cbl_nullable outError) noexcept {
    try {
        CBLDatabase::enableVectorSearch(path);
        return true;
    } catchAndBridge(outError)
}

#endif
