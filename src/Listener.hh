//
// Listener.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

#pragma once
#include "CBLDatabase.h"
#include "Internal.hh"
#include "fleece/InstanceCounted.hh"
#include <access_lock.hh>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include "betterassert.hh"

CBL_ASSUME_NONNULL_BEGIN

namespace cbl_internal {
    class ListenersBase;
}


/** Abstract base class of listener tokens. (In the public API, as an opaque typedef.) */
struct CBLListenerToken : public CBLRefCounted {
public:
    CBLListenerToken(const void* callback, void* _cbl_nullable context)
    :_callback(callback)
    ,_context(context)
    { }

    virtual ~CBLListenerToken() {
        if (_extraInfo.destructor) {
            _extraInfo.destructor(_extraInfo.pointer);
        }
    }

    void addedTo(cbl_internal::ListenersBase *owner) {
        assert(!_owner);
        _owner = owner;
    }

    /** Called by `CBLListener_Remove` */
    void remove();
    
    /** Called by the remove() function before removing the token from the owner.
        Subclasses can override this function to perform any tasks such as
        stopping the underlinging observer and etc before the token is removed. */
    virtual void willRemove() { }

    /** For attaching some extra info. For example, use the extraInfo for keeping a listener
        and its context when wrapping the to pass the listener to another listener. */
    C4ExtraInfo& extraInfo()                                {return _extraInfo;}
    const C4ExtraInfo& extraInfo() const                    {return _extraInfo;}

protected:
    friend class cbl_internal::ListenersBase;

    void removed() {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _owner = nullptr;
        _callback = nullptr;
    }

    /** Must be held when accessing _callback and while running it.
        https://github.com/couchbase/couchbase-lite-C/pull/372 */
    std::recursive_mutex                       _mutex;
    const void*  _cbl_nullable                 _callback;          // Really a C fn pointer
    void* const  _cbl_nullable                 _context;
    C4ExtraInfo                                _extraInfo = {};

    cbl_internal::ListenersBase* _cbl_nullable _owner {nullptr};
};


namespace cbl_internal {

    /** Type-safe CBLListenerToken. Thread-safe. */
    template <class LISTENER>
    struct ListenerToken : public CBLListenerToken {
    public:
        ListenerToken(LISTENER callback, void* _cbl_nullable context)
        :CBLListenerToken((const void*)callback, context)
        { }

        LISTENER callback() const           {return (LISTENER)_callback;}

        template <class... Args>
        void call(Args... args) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            LISTENER cb = callback();
            if (cb)
                cb(_context, args...);
        }
    };



    /** Manages a set of CBLListenerTokens. Thread-safe. */
    class ListenersBase {
    public:
        ~ListenersBase() {
            clear();
        }

        void add(CBLListenerToken* t) {
            LOCK(_mutex);
            _tokens.emplace_back(t);
            t->addedTo(this);
        }

        void remove(CBLListenerToken* t) {
            LOCK(_mutex);
            for (auto i = _tokens.begin(); i != _tokens.end(); ++i) {
                if (i->get() == t) {
                    _tokens.erase(i);
                    return;
                }
            }
            assert(false);
        }

        void clear() {
            LOCK(_mutex);
            for (auto &tok : _tokens)
                tok->removed();
            _tokens.clear();
        }

        bool contains(CBLListenerToken *token) const {
            LOCK(_mutex);
            for (auto &tok : _tokens) {
                if (tok == token)
                    return true;
            }
            return false;
        }

        bool empty() const {
            LOCK(_mutex);
            return _tokens.empty();
        }

        using Tokens = std::vector<fleece::Retained<CBLListenerToken>>;

        Tokens tokens() const {
            LOCK(_mutex);
            return _tokens;
        }

    private:
        mutable std::mutex _mutex;
        Tokens _tokens;
    };


    /** Manages a set of ListenerTokens. Thread-safe. */
    template <class LISTENER>
    class Listeners : private ListenersBase {
    public:
        fleece::Retained<CBLListenerToken> add(LISTENER listener, void* _cbl_nullable context) {
            auto t = new ListenerToken<LISTENER>(listener, context);
            add(t);
            return t;
        }

        void add(ListenerToken<LISTENER>* _cbl_nonnull token)                {ListenersBase::add(token);}
        void clear()                                            {ListenersBase::clear();}
        bool empty() const                                      {return ListenersBase::empty();}

        ListenerToken<LISTENER>* _cbl_nullable find(CBLListenerToken *token) const {
            return contains(token) ? (ListenerToken<LISTENER>*) token : nullptr;
        }

        template <class... Args>
        void call(Args... args) const {
            for (auto &lp : tokens())
                ((ListenerToken<LISTENER>*)lp.get())->call(args...);
        }
    };


    using Notification = std::function<void()>;


    /** Manages a queue of pending calls to listeners. Owned by CBLDatabase. Thread-safe. */
    class NotificationQueue {
    public:
        NotificationQueue(CBLDatabase*);

        /** Sets or clears the client callback. */
        void setCallback(CBLNotificationsReadyCallback _cbl_nullable callback, void* _cbl_nullable context);

        /** If there is a callback, this adds a notification to the queue, and if the queue was
            empty, invokes the callback to tell the client.
            If there is no callback, it calls the notification directly. */
        void add(Notification);

        /** Calls all queued notifications and clears the queue. */
        void notifyAll();


    private:
        using Notifications = std::unique_ptr<std::vector<Notification>>;

        void call(const Notifications&);

        struct State {
            CBLNotificationsReadyCallback _cbl_nullable callback {nullptr};
            void* _cbl_nullable context;
            Notifications queue;
        };

        CBLDatabase* const _database;
        litecore::access_lock<State> _state;
    };

}

CBL_ASSUME_NONNULL_END
