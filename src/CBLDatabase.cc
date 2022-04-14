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
#include "CBLBlob_Internal.hh"
#include "CBLDocument_Internal.hh"
#include "CBLQuery_Internal.hh"
#include "CBLPrivate.h"
#include "c4Observer.hh"
#include "c4Query.hh"
#include "Internal.hh"
#include "fleece/function_ref.hh"
#include "fleece/PlatformCompat.hh"
#include <sys/stat.h>

#ifndef CMAKE
#include <unistd.h>
#endif

using namespace std;
using namespace fleece;
using namespace cbl_internal;


#pragma mark - CONFIGURATION:


// Default location for databases. This is platform-dependent.
// * Apple : CBLDatabase+Apple.mm
// * Android : CBLDatabase+Android.cc
#if !defined(__APPLE__) && !defined(__ANDROID__)
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

bool CBLEncryptionKey_FromPasswordOld(CBLEncryptionKey *key, FLString password) CBLAPI {
    auto c4key = C4EncryptionKeyFromPasswordSHA1(password, kC4EncryptionAES256);    //FIXME: Catch
    key->algorithm = CBLEncryptionAlgorithm(c4key.algorithm);
    memcpy(key->bytes, c4key.bytes, sizeof(key->bytes));
    return true;
}
#endif


#pragma mark - QUERY:


Retained<CBLQuery> CBLDatabase::createQuery(CBLQueryLanguage language,
                                            slice queryString,
                                            int* _cbl_nullable outErrPos) const
{
    alloc_slice json;
    if (language == kCBLJSONLanguage) {
        json = convertJSON5(queryString); // allow JSON5 as a convenience
        queryString = json;
    }
    auto c4query = _c4db.useLocked()->newQuery((C4QueryLanguage)language, queryString, outErrPos);
    if (!c4query)
        return nullptr;
    return new CBLQuery(this, std::move(c4query), _c4db);
}


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
            _c4obs = c4db->getDefaultCollection()->observeDocument(docID,
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

        Retained<CBLDatabase> _db;
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


#pragma mark - BINDING DEV SUPPORT FOR BLOB


Retained<CBLBlob> CBLDatabase::getBlob(FLDict properties) {
    try {
        return new CBLBlob(this, properties);
    } catch (...) {
        C4Error err = C4Error::fromCurrentException();
        if (err == C4Error{LiteCoreDomain, kC4ErrorNotFound})
            return nullptr;
        throw;
    }
}


void CBLDatabase::saveBlob(CBLBlob* blob) {
    blob->install(this);
}
