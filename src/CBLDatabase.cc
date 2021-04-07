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
#include "CBLDocument_Internal.hh"
#include "CBLQuery_Internal.hh"
#include "CBLPrivate.h"
#include "c4Observer.hh"
#include "c4Query.hh"
#include "Internal.hh"
#include "function_ref.hh"
#include "PlatformCompat.hh"
#include <sys/stat.h>

#ifndef CMAKE
#include <unistd.h>
#endif

using namespace std;
using namespace fleece;
using namespace cbl_internal;


#pragma mark - CONFIGURATION:


// Default location for databases. This is platform-dependent.
// (The implementation for Apple platforms is in CBLDatabase+Apple.mm)
#ifndef __APPLE__
std::string CBLDatabase::defaultDirectory() {
    return cbl_getcwd(nullptr, 0);
}
#endif


#ifdef COUCHBASE_ENTERPRISE
static_assert(sizeof(CBLEncryptionKey::bytes) == sizeof(C4EncryptionKey::bytes),
              "C4EncryptionKey and CBLEncryptionKey size do not match");

bool CBLEncryptionKey_FromPassword(CBLEncryptionKey *key, FLString password) CBLAPI {
    auto c4key = C4EncryptionKeyFromPassword(password, kC4EncryptionAES256);    //FIXME: Catch
    key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
    memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
    return true;
}
#endif


#pragma mark - QUERY:


namespace cbl_internal {

    void ListenerToken<CBLQueryChangeListener>::queryChanged() {
        _query->database()->notify(this);
    }


#pragma mark - DOCUMENT LISTENERS:


    // Custom subclass of CBLListenerToken for document listeners.
    // (It implements the ListenerToken<> template so that it will work with Listeners<>.)
    template<>
    struct ListenerToken<CBLDocumentChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLDatabase *db, slice docID, CBLDocumentChangeListener callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_db(db)
        ,_docID(docID)
        {
            auto c4db = _db->useLocked(); // locks DB mutex
            _c4obs = c4db->observeDocument(docID,
                                         [this](C4DocumentObserver*, slice docID, C4SequenceNumber)
                                         {
                                             this->docChanged();
                                         });
        }

        ~ListenerToken() {
            auto c4db = _db->useLocked(); // locks DB mutex
            _c4obs = nullptr;
        }

        CBLDocumentChangeListener callback() const {
            return (CBLDocumentChangeListener)_callback.load();
        }

        // this is called indirectly by CBLDatabase::sendNotifications
        void call(const CBLDatabase*, FLString) {
            auto cb = callback();
            if (cb)
                cb(_context, _db, _docID);
        }

    private:
        void docChanged() {
            _db->notify(this, _db, _docID);
        }

        CBLDatabase* _db;
        alloc_slice _docID;
        unique_ptr<C4DocumentObserver> _c4obs;
    };

}


Retained<CBLListenerToken> CBLDatabase::addDocListener(slice docID,
                                                       CBLDocumentChangeListener listener,
                                                       void *context)
{
    auto token = new ListenerToken<CBLDocumentChangeListener>(this, docID, listener, context);
    _docListeners.add(token);
    return token;
}
