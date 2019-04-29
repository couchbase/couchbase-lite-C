//
// Listener.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "CBLDatabase.h"
#include "Internal.hh"
#include <access_lock.hh>
#include <atomic>
#include <memory>
#include <vector>


namespace cbl_internal {
    class ListenersBase;
}


/** Abstract base class of listener tokens. (In the public API, as an opaque typeef.) */
struct CBLListenerToken : public fleece::RefCounted {
public:
    CBLListenerToken(const void *callback _cbl_nonnull, void *context)
    :_callback(callback)
    ,_context(context)
    { }

    virtual ~CBLListenerToken()  { }

    void addedTo(cbl_internal::ListenersBase *owner _cbl_nonnull) {
        assert(!_owner);
        _owner = owner;
    }

    /** Called by `CBLListener_Remove` */
    void remove();

protected:
    std::atomic<const void*> _callback;          // Really a C fn pointer
    void* _context;
    cbl_internal::ListenersBase* _owner {nullptr};
};


namespace cbl_internal {

    /** Type-safe CBLListenerToken */
    template <class LISTENER>
    class ListenerToken : public CBLListenerToken {
    public:
        ListenerToken(LISTENER callback, void *context)
        :CBLListenerToken((const void*)callback, context)
        { }

        LISTENER callback() const           {return (LISTENER)_callback.load();}

            template <class... Args>
        void call(Args... args) {
            LISTENER cb = callback();
            if (cb)
                cb(_context, args...);
        }
    };



    /** Manages a set of CBLListenerTokens. */
    class ListenersBase {
    public:
        void add(CBLListenerToken* t _cbl_nonnull) {
            _tokens.emplace_back(t);
            t->addedTo(this);
        }

        void remove(CBLListenerToken* t _cbl_nonnull) {
            for (auto i = _tokens.begin(); i != _tokens.end(); ++i) {
                if (i->get() == t) {
                    _tokens.erase(i);
                    return;
                }
            }
            assert(false);
        }

        void clear() {
            _tokens.clear();
        }

        bool contains(CBLListenerToken *token _cbl_nonnull) const {
            for (auto &tok : _tokens) {
                if (tok == token)
                    return true;
            }
            return false;
        }

    protected:
        std::vector<fleece::Retained<CBLListenerToken>> _tokens;
    };


    /** Manages a set of ListenerTokens. */
    template <class LISTENER>
    class Listeners : private ListenersBase {
    public:
        CBLListenerToken* add(LISTENER listener, void *context) {
            auto t = new ListenerToken<LISTENER>(listener, context);
            add(t);
            return t;
        }

        void add(ListenerToken<LISTENER> *token)                {ListenersBase::add(token);}
        void clear()                                            {ListenersBase::clear();}

        ListenerToken<LISTENER>* find(CBLListenerToken *token) {
            return contains(token) ? (ListenerToken<LISTENER>*) token : nullptr;
        }

            template <class... Args>
        void call(Args... args) {
            for (auto &lp : _tokens)
                ((ListenerToken<LISTENER>*)lp.get())->call(args...);
        }
    };


    using Notification = std::function<void()>;


    /** Manages a queue of pending calls to listeners. Owned by CBLDatabase. */
    class NotificationQueue {
    public:
        NotificationQueue(CBLDatabase* _cbl_nonnull);

        /** Sets or clears the client callback. */
        void setCallback(CBLNotificationsReadyCallback callback, void *context);

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
            CBLNotificationsReadyCallback callback {nullptr};
            void* context;
            Notifications queue;
        };

        CBLDatabase* const _database;
        litecore::access_lock<State> _state;
    };

}
