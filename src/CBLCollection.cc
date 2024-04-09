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
    struct ListenerToken<CBLCollectionChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLCollection *collection, CBLCollectionChangeListener callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_collection(collection)
        ,_database(collection->database()) { }
        
        ~ListenerToken() { }
        
        CBLCollectionChangeListener _cbl_nullable callback() const {
            return (CBLCollectionChangeListener)_callback;
        }
        
        void call(const CBLCollectionChange* change) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            auto cb = callback();
            if (cb) {
                cb(_context, change);
            }
        }
    private:
        Retained<CBLCollection> _collection;
        Retained<CBLDatabase> _database;
    };

    template<>
    struct ListenerToken<CBLCollectionDocumentChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLCollection *collection, slice docID,
                      CBLCollectionDocumentChangeListener callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_collection(collection)
        ,_database(collection->database())
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

        CBLCollectionDocumentChangeListener _cbl_nullable callback() const {
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
            _database->notify(this, change);
        }

        Retained<CBLCollection> _collection;
        Retained<CBLDatabase> _database;
        alloc_slice _docID;
        std::unique_ptr<C4DocumentObserver> _c4obs;
    };
}

Retained<CBLListenerToken> CBLCollection::addChangeListener(CBLCollectionChangeListener listener,
                                                            void* _cbl_nullable ctx)
{
    auto lock =_c4col.useLocked(); // Ensure the database lifetime while creating the Listener oken
    auto token = addListener([&] { return new ListenerToken<CBLCollectionChangeListener>(this, listener, ctx); });
    _listeners.add((ListenerToken<CBLCollectionChangeListener>*)token.get());
    return token;
}

Retained<CBLListenerToken> CBLCollection::addDocumentListener(slice docID, 
                                                              CBLCollectionDocumentChangeListener listener,
                                                              void* _cbl_nullable ctx)
{
    auto lock =_c4col.useLocked(); // // Ensure the database lifetime while creating the Listener oken
    auto token = new ListenerToken<CBLCollectionDocumentChangeListener>(this, docID, listener, ctx);
    _docListeners.add(token);
    return token;
}
