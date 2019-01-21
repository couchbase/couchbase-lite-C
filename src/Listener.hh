//
// Listener.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "CBLBase.h"
#include "Internal.hh"


namespace cbl_internal {
    class ListenersBase;
}


/** Abstract base class of listener tokens. (In the public API, as an opaque typeef.) */
struct CBLListenerToken {
public:
    CBLListenerToken(const void *callback, void *context)
    :_callback(callback)
    ,_context(context)
    { }

    virtual ~CBLListenerToken()  { }

    void addedTo(cbl_internal::ListenersBase *owner _cbl_nonnull) {
        assert(!_owner);
        _owner = owner;
    }

    /** Called by `cbl_listener_remove` */
    void remove();

protected:
    const void* _callback;          // Really a C fn pointer
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

        LISTENER callback() const           {return (LISTENER)_callback;}

            template <class... Args>
        void call(Args... args)             {callback()(_context, args...);}
    };


    /** Manages a set of CBLListenerTokens. */
    class ListenersBase {
    public:
        void add(CBLListenerToken* t) {
            _tokens.emplace_back(t);
            t->addedTo(this);
        }

        void remove(CBLListenerToken* t) {
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

    protected:
        std::vector<std::unique_ptr<CBLListenerToken>> _tokens;
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

            template <class... Args>
        void call(Args... args) {
            for (auto &lp : _tokens)
                ((ListenerToken<LISTENER>*)lp.get())->call(args...);
        }
    };

}
