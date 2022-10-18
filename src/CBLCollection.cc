//
//  CBLCollection.cc
//
// Copyright (c) 2022 Couchbase, Inc All rights reserved.
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

#include "CBLCollection_Internal.hh"
#include "Internal.hh"

using namespace fleece;
using namespace cbl_internal;

namespace cbl_internal {

    template<>
    struct ListenerToken<CBLCollectionDocumentChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLCollection *collection, slice docID,
                      CBLCollectionDocumentChangeListener callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_collection(collection)
        ,_docID(docID)
        {
            _c4obs = _collection->useLocked()->observeDocument(docID, [this](C4DocumentObserver*,
                                                                             C4Collection*,
                                                                             slice,
                                                                             C4SequenceNumber) {
                this->docChanged();
            });
        }

        ~ListenerToken() {
            try {
                auto lock = _collection->useLocked();
                _c4obs = nullptr;
            } catch (...) {
                // Collection is deleted or database is released:
                _c4obs = nullptr;
            }
        }

        CBLCollectionDocumentChangeListener callback() const {
            return (CBLCollectionDocumentChangeListener)_callback;
        }

        // this is called indirectly by CBLDatabase::sendNotifications
        void call(CBLDocumentChange change) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            auto cb = callback();
            if (cb) {
                cb(_context, &change);
            }
        }

    private:
        void docChanged() {
            CBLDocumentChange change = {};
            change.collection = _collection;
            change.docID = _docID;

            Retained<CBLDatabase> db;
            try {
                db = _collection->database();
            } catch (...) {
                C4Error error = C4Error::fromCurrentException();
                CBL_Log(kCBLLogDomainDatabase, kCBLLogWarning,
                        "Document changed notification failed: %s", error.description().c_str());
            }

            if (db) {
                db->notify(this, change);
            }
        }

        Retained<CBLCollection> _collection;
        alloc_slice _docID;
        std::unique_ptr<C4DocumentObserver> _c4obs;
    };

}

Retained<CBLListenerToken>
CBLCollection::addDocumentListener(slice docID, CBLCollectionDocumentChangeListener listener,
                                   void* _cbl_nullable ctx)
{
    auto token = new ListenerToken<CBLCollectionDocumentChangeListener>(this, docID, listener, ctx);
    _docListeners.add(token);
    return token;
}
