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

#include "CBLDatabase_Internal.hh"
#include "CBLDocument.h"
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


static string dbPath(const char* _cbl_nonnull name, const char *inDirectory) {
    if (!inDirectory)
        inDirectory = defaultDbDir();
    string path(inDirectory);
    if (path[path.size()] != '/')
        path += '/';
    path += name;
    path += ".cblite2";
    return path;
}


static string dbPath(const char* _cbl_nonnull name, const CBLDatabaseConfiguration *config) {
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


#pragma mark - DATABASE LISTENERS:


CBLDatabase::~CBLDatabase() {
    c4dbobs_free(_observer);
    _docListeners.clear();
    c4db_release(c4db);
}


CBLListenerToken* CBLDatabase::addListener(CBLDatabaseListener listener, void *context) {
    auto token = _listeners.add(listener, context);
    if (!_observer) {
        _observer = c4dbobs_create(c4db,
                                   [](C4DatabaseObserver* observer, void *context) {
                                       ((CBLDatabase*)context)->callListeners();
                                   },
                                   this);
    }
    return token;
}


void CBLDatabase::callListeners() {
    static const uint32_t kMaxChanges = 100;
    while (true) {
        C4DatabaseChange c4changes[kMaxChanges];
        bool external;
        uint32_t nChanges = c4dbobs_getChanges(_observer, c4changes, kMaxChanges, &external);
        if (nChanges == 0)
            break;
        // Convert docID slices to C strings:
        const char* docIDs[kMaxChanges];
        size_t bufSize = 0;
        for (uint32_t i = 0; i < nChanges; ++i)
            bufSize += c4changes[i].docID.size + 1;
        char *buf = new char[bufSize], *next = buf;
        for (uint32_t i = 0; i < nChanges; ++i) {
            docIDs[i] = next;
            memcpy(next, (const char*)c4changes[i].docID.buf, c4changes[i].docID.size);
            next += c4changes[i].docID.size;
            *(next++) = '\0';
        }
        assert(next - buf == bufSize);
        // Call the listener(s):
        _listeners.call(this, docIDs);
        delete [] buf;
    }
}


CBLListenerToken* cbl_db_addListener(const CBLDatabase* constdb _cbl_nonnull,
                                     CBLDatabaseListener listener _cbl_nonnull,
                                     void *context)
{
    return const_cast<CBLDatabase*>(constdb)->addListener(listener, context);
}


#pragma mark - DOCUMENT LISTENERS:


namespace cbl_internal {

    class DocListenerToken : public ListenerToken<CBLDocumentListener> {
    public:
        DocListenerToken(CBLDatabase *db, const char *docID, CBLDocumentListener callback, void *context)
        :ListenerToken(callback, context)
        ,_db(db)
        ,_docID(docID)
        ,_c4obs( c4docobs_create(internal(db),
                                 slice(docID),
                                 [](C4DocumentObserver* observer, C4String docID,
                                    C4SequenceNumber sequence, void *context)
                                 {
                                     ((DocListenerToken*)context)->callListener();
                                 },
                                 this) )
        { }

        ~DocListenerToken() {
            c4docobs_free(_c4obs);
        }

    private:
        void callListener() {
            call(_db, _docID.c_str());
        }

        CBLDatabase* _db;
        string _docID;
        C4DocumentObserver* _c4obs {nullptr};
    };

}


CBLListenerToken* CBLDatabase::addDocListener(const char* docID _cbl_nonnull,
                                              CBLDocumentListener listener, void *context)
{
    auto token = new DocListenerToken(this, docID, listener, context);
    _docListeners.add(token);
    return token;
}


CBLListenerToken* cbl_db_addDocumentListener(const CBLDatabase* db _cbl_nonnull,
                                             const char* docID _cbl_nonnull,
                                             CBLDocumentListener listener _cbl_nonnull,
                                             void *context)
{
    return const_cast<CBLDatabase*>(db)->addDocListener(docID, listener, context);

}
