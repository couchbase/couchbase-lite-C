//
// CBLDatabase.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "CBLDatabase.h"
#include "Internal.hh"
#include "Util.hh"
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using namespace fleece;
using namespace cbl_internal;


static const char* defaultDbDir() {
    static const string kDir = string(getcwd(nullptr, 0)) + "/";    // FIX COMPAT
    return kDir.c_str();
}


static string dbPath(const char* _cblnonnull name, const char *inDirectory) {
    if (!inDirectory)
        inDirectory = defaultDbDir();
    string path(inDirectory);
    if (path[path.size()] != '/')
        path += '/';
    path += name;
    path += ".cblite2";
    return path;
}


static string dbPath(const char* _cblnonnull name, const CBLDatabaseConfiguration *config) {
    return dbPath(name, (config ? config->directory : nullptr));
}


bool cbl_databaseExists(const char *name, const char *inDirectory) {
    string path = dbPath(name, inDirectory);
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}


bool cbl_copyDB(const char* fromPath,
                const char* toName,
                const CBLDatabaseConfiguration *config,
                CBLError* outError)
{
    string toPath = dbPath(toName, config);
    C4DatabaseConfig c4config {kC4DB_Create | kC4DB_AutoCompact | kC4DB_SharedKeys};
    return c4db_copy(slice(fromPath), slice(toPath), &c4config, internal(outError));
}


bool cbl_deleteDB(const char *name,
                  const char *inDirectory,
                  CBLError* outError)
{
    string path = dbPath(name, inDirectory);
    return c4db_deleteAtPath(slice(path), internal(outError));
}


CBLDatabase* cbl_db_open(const char *name,
                         const CBLDatabaseConfiguration *config,
                         CBLError *outError)
{
    C4Log("name = %s  dir = %s", name, config->directory);//TEMP
    string path = dbPath(name, config);
    C4Log("path = %s", path.c_str());//TEMP
    C4DatabaseConfig c4config {kC4DB_Create | kC4DB_AutoCompact | kC4DB_SharedKeys};
    C4Database *c4db = c4db_open(slice(path), &c4config, internal(outError));
    if (!c4db)
        return nullptr;
    return retain(new CBLDatabase(c4db, name, path, (config->directory ?: "")));
}


bool cbl_db_close(CBLDatabase* db, CBLError* outError) {
    return !db || c4db_close(internal(db), internal(outError));
}


const char* cbl_db_name(const CBLDatabase* db) {
    return db->name.c_str();
}

const char* cbl_db_path(const CBLDatabase* db) {
    return db->path.c_str();
}

const CBLDatabaseConfiguration cbl_db_config(const CBLDatabase* db) {
    const char *dir = db->dir.empty() ? nullptr : db->dir.c_str();
    return {dir};
}

uint64_t cbl_db_count(const CBLDatabase* db) {
    return c4db_getDocumentCount(internal(db));
}

uint64_t cbl_db_lastSequence(const CBLDatabase* db) {
    return c4db_getLastSequence(internal(db));
}

bool cbl_db_beginBatch(CBLDatabase* db, CBLError* outError) {
    return c4db_beginTransaction(internal(db), internal(outError));
}

bool cbl_db_endBatch(CBLDatabase* db, CBLError* outError) {
    return c4db_endTransaction(internal(db), true, internal(outError));
}

bool cbl_db_compact(CBLDatabase* db, CBLError* outError) {
    return c4db_compact(internal(db), internal(outError));
}

bool cbl_db_delete(CBLDatabase* db, CBLError* outError) {
    return c4db_delete(internal(db), internal(outError));
}
