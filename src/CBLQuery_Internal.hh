//
// CBLQuery_Internal.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "CBLQuery.h"
#include "CBLDatabase_Internal.hh"
#include "Internal.hh"
#include "Listener.hh"
#include "c4Query.hh"
#include "access_lock.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <optional>
#include <unordered_map>

using namespace std;
using namespace fleece;

CBL_ASSUME_NONNULL_BEGIN


#pragma mark - QUERY CLASS:


struct CBLQuery final : public CBLRefCounted {
public:

    ~CBLQuery() {
        _c4query.useLocked().get() = nullptr;
    }

    const CBLDatabase* database() const {
        return _database;
    }

    alloc_slice explain() const {
        return _c4query.useLocked()->explain();
    }

    unsigned columnCount() const {
        return _c4query.useLocked()->columnCount();
    }

    slice columnName(unsigned col) const {
        return _c4query.useLocked()->columnTitle(col);
    }

    Dict parameters() const {
        if (!_parameters)
            return nullptr;
        return Value::fromData(_parameters, kFLTrusted).asDict();
    }

    void setParameters(Dict parameters) {
        Encoder enc;
        enc.writeValue(parameters);
        _encodeParameters(enc);
    }

    void setParametersAsJSON(slice json5) {
        Encoder enc;
        enc.convertJSON(convertJSON5(json5));
        _encodeParameters(enc);
    }

    inline Retained<CBLResultSet> execute();

    using ColumnNamesMap = unordered_map<slice, uint32_t>;

    int columnNamed(slice name) const {
        call_once(_onceColumnNames, [this]{
            ColumnNamesMap names;
            unsigned nCols = columnCount();
            names.reserve(nCols);
            for (unsigned col = 0; col < nCols; ++col)
                names.insert({columnName(col), col});
            _columnNames = names;
        });
        auto i = _columnNames->find(name);
        return (i != _columnNames->end()) ? i->second : -1;
    }

    inline Retained<CBLListenerToken> addChangeListener(CBLQueryChangeListener listener,
                                                        void* _cbl_nullable context);

    ListenerToken<CBLQueryChangeListener>* getChangeListener(CBLListenerToken *token) const {
        return _listeners.find(token);
    }

private:
    friend struct CBLDatabase;
    friend struct cbl_internal::ListenerToken<CBLQueryChangeListener>;

    CBLQuery(const CBLDatabase *db,
             Retained<C4Query>&& c4query,
             const litecore::access_lock<Retained<C4Database>> &owner)
    :_c4query(std::move(c4query), owner)
    ,_database(db)
    { }

    void _encodeParameters(Encoder &enc) {
        alloc_slice encodedParameters = enc.finish();
        if (!encodedParameters)
            C4Error::raise(FleeceDomain, enc.error(), "%s", enc.errorMessage());
        _parameters = encodedParameters;
        _c4query.useLocked()->setParameters(encodedParameters);
    }

    litecore::shared_access_lock<Retained<C4Query>> _c4query;// Thread-safe access to C4Query
    RetainedConst<CBLDatabase>          _database;          // Owning database
    alloc_slice                         _parameters;        // Fleece-encoded param values
    mutable optional<ColumnNamesMap>    _columnNames;       // Maps colum name to index
    mutable once_flag                   _onceColumnNames;   // For lazy init of _columnNames
    Listeners<CBLQueryChangeListener>   _listeners;         // Query listeners
};


#pragma mark - RESULT SET CLASS:


struct CBLResultSet final : public CBLRefCounted {
public:
    CBLResultSet(CBLQuery* query, C4Query::Enumerator qe)
    :_query(query)
    ,_enum(move(qe))
    { }

    bool next() {
        _asArray = nullptr;
        _asDict = nullptr;
        return _enum.next();
    }

    Value property(slice prop) const {
        int col = _query->columnNamed(prop);
        return (col >= 0) ? column(col) : nullptr;
    }

    Value column(unsigned col) const {
        return _enum.column(col);
    }

    Array asArray() const {
        if (!_asArray) {
            auto array = MutableArray::newArray();
            unsigned nCols = _query->columnCount();
            array.resize(uint32_t(nCols));
            for (unsigned i = 0; i < nCols; ++i) {
                Value val = column(i);
                array[i] = val ? val : Value::null();
            }
            _asArray = array;
        }
        return _asArray;
    }

    Dict asDict() const {
        if (!_asDict) {
            auto dict = MutableDict::newDict();
            unsigned nCols = _query->columnCount();
            for (unsigned i = 0; i < nCols; ++i) {
                if (Value val = column(i); val) {
                    slice key = _query->columnName(i);
                    dict[key] = val;
                }
            }
            _asDict = dict;
        }
        return _asDict;
    }

    CBLQuery* query() const {
        return _query;
    }

private:
    Retained<CBLQuery> const    _query;    // The query
    C4Query::Enumerator         _enum;     // The query enumerator
    mutable MutableArray        _asArray;  // Column values as a Fleece Array
    mutable MutableDict         _asDict;   // Column names/values as a Fleece Dict
};


#pragma mark - QUERY LISTENER:


namespace cbl_internal {

    // Custom subclass of CBLListenerToken for query listeners.
    // (It implements the ListenerToken<> template so that it will work with Listeners<>.)
    template<>
    struct ListenerToken<CBLQueryChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLQuery *query,
                      CBLQueryChangeListener callback,
                      void* _cbl_nullable context)
        :CBLListenerToken((const void*)callback, context)
        ,_query(query)
        ,_notifyLock(this)
        {
            query->_c4query.useLocked([&](C4Query *c4query) {
                _c4obs = c4query->observe([this](C4QueryObserver*) { this->queryChanged(); });
            });
        }

        void setEnabled(bool enabled) {
            _query->_c4query.useLocked([&](C4Query *c4query) {
                _c4obs->setEnabled(enabled);
            });
        }

        CBLQueryChangeListener callback() const {
            return (CBLQueryChangeListener)_callback.load();
        }

        void call() {
            CBLQueryChangeListener cb = callback();
            if (cb) {
                auto locked = _notifyLock.useLocked();
                cb(_context, _query);
            }
        }
        
        auto useNotifyLocked() {
            return _notifyLock.useLocked();
        }

        Retained<CBLResultSet> resultSet() {
            return new CBLResultSet(_query, _c4obs->getEnumerator(false));
        }

    private:
        void queryChanged();    // defn is in CBLDatabase.cc, to prevent circular hdr dependency

        Retained<CBLQuery>  _query;
        std::unique_ptr<C4QueryObserver> _c4obs;
        litecore::access_lock<ListenerToken*> _notifyLock;
    };

}


inline Retained<CBLResultSet> CBLQuery::execute() {
    auto qe = _c4query.useLocked()->run();
    return retained(new CBLResultSet(this, move(qe)));
}


inline Retained<CBLListenerToken> CBLQuery::addChangeListener(CBLQueryChangeListener listener,
                                                              void* _cbl_nullable context)
{
    auto token = retained(new ListenerToken<CBLQueryChangeListener>(this, listener, context));
    _listeners.add(token);
    
    // Make sure that the notification doesn't happen before the token is returned:
    auto locked = token->useNotifyLocked();
    token->setEnabled(true);
    return token;
}

CBL_ASSUME_NONNULL_END
