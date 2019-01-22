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


#ifdef _MSC_VER
static const char kPathSeparator = '\\';
#else
static const char kPathSeparator = '/';
#endif


// Default location for databases: the current directory
static const string& defaultDbDir() {
    static const string kDir = getcwd(nullptr, 0);
    return kDir;
}


// Given a database name and an optional directory, returns the complete path.
// * If no directory is given, uses the defaultDbDir().
// * Appends ".cblite2" to the database name.
static string dbPath(const char* _cbl_nonnull name, const char *inDirectory) {
    string path;
    if (inDirectory)
        path = inDirectory;
    else
        path = defaultDbDir();
    if (path[path.size()-1] != kPathSeparator)
        path += kPathSeparator;
    return path + name + ".cblite2";
}


static string dbPath(const char* _cbl_nonnull name, const CBLDatabaseConfiguration *config) {
    return dbPath(name, (config ? config->directory : nullptr));
}


#pragma mark - STATIC "METHODS":


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


#pragma mark - LIFECYCLE & OPERATIONS:


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


#pragma mark - ACCESSORS:


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


#pragma mark - DATABASE LISTENERS:


CBLDatabase::~CBLDatabase() {
    c4dbobs_free(_observer);
    _docListeners.clear();
    c4db_release(c4db);
}


bool CBLDatabase::shouldNotifyNow() {
    if (_notificationsCallback) {
        if (!_notificationsAnnounced) {
            _notificationsAnnounced = true;
            _notificationsCallback(_notificationsContext, this);
        }
        return false;
    } else {
        return true;
    }
}


CBLListenerToken* CBLDatabase::addListener(CBLDatabaseListener listener, void *context) {
    auto token = _listeners.add(listener, context);
    if (!_observer) {
        _observer = c4dbobs_create(c4db,
                                   [](C4DatabaseObserver* observer, void *context) {
                                       ((CBLDatabase*)context)->databaseChanged();
                                   },
                                   this);
    }
    return token;
}


void CBLDatabase::databaseChanged() {
    if (shouldNotifyNow())
        callDBListeners();
}


void CBLDatabase::callDBListeners() {
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
        _listeners.call(this, nChanges, docIDs);
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

    // Custom subclass of CBLListenerToken for document listeners.
    // (It implements the ListenerToken<> template so that it will work with Listeners<>.)
    template<>
    class ListenerToken<CBLDocumentListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLDatabase *db, const char *docID, CBLDocumentListener callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_db(db)
        ,_docID(docID)
        ,_c4obs( c4docobs_create(internal(db),
                                 slice(docID),
                                 [](C4DocumentObserver* observer, C4String docID,
                                    C4SequenceNumber sequence, void *context)
                                 {
                                     ((ListenerToken*)context)->docChanged();
                                 },
                                 this) )
        { }

        ~ListenerToken() {
            c4docobs_free(_c4obs);
        }

        CBLDocumentListener callback() const           {return (CBLDocumentListener)_callback;}

        // this is called indirectly by CBLDatabase::sendNotifications
        void call(const CBLDatabase*, const char*) {
            if (_scheduled) {
                _scheduled = false;
                callback()(_context, _db, _docID.c_str());
            }
        }

    private:
        void docChanged() {
            _scheduled = true;
            if (_db->shouldNotifyNow())
                call(nullptr, nullptr);
        }

        CBLDatabase* _db;
        string _docID;
        C4DocumentObserver* _c4obs {nullptr};
        bool _scheduled {false};
    };

}


CBLListenerToken* CBLDatabase::addDocListener(const char* docID _cbl_nonnull,
                                              CBLDocumentListener listener, void *context)
{
    auto token = new ListenerToken<CBLDocumentListener>(this, docID, listener, context);
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


#pragma mark - SCHEDULING NOTIFICATIONS:


void CBLDatabase::bufferNotifications(CBLNotificationsReadyCallback callback, void *context) {
    _notificationsContext = context;
    _notificationsCallback = callback;
}


void CBLDatabase::sendNotifications() {
    if (!_notificationsAnnounced)
        return;
    _notificationsAnnounced = false;
    callDBListeners();
    _docListeners.call(this, nullptr);
}



void cbl_db_bufferNotifications(CBLDatabase *db,
                                CBLNotificationsReadyCallback callback,
                                void *context)
{
    db->bufferNotifications(callback, context);
}

void cbl_db_sendNotifications(CBLDatabase *db) {
    db->sendNotifications();
}

