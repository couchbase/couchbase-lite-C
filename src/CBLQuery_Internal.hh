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
#include "Util.hh"
#include "c4.hh"
#include "c4Query.h"
#include "access_lock.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <optional>
#include <unordered_map>

using namespace std;
using namespace fleece;


#pragma mark - QUERY CLASS:


class CBLQuery : public CBLRefCounted, public litecore::shared_access_lock<C4Query*> {
public:

    CBLQuery(const CBLDatabase* db _cbl_nonnull,
             CBLQueryLanguage language,
             slice queryString,
             int *outErrPos,
             C4Error* outError)
    :shared_access_lock<C4Query*>(createC4Query(db, language, queryString, outErrPos, outError), db)
    ,_database(db)
    { }

    ~CBLQuery() {
        use([](C4Query *c4query) {
            c4query_release(c4query);
        });
    }

    static C4Query* createC4Query(const CBLDatabase* db _cbl_nonnull,
                                  CBLQueryLanguage language,
                                  slice queryString,
                                  int *outErrPos,
                                  C4Error* outError)
    {
        alloc_slice json;
        if (language == kCBLJSONLanguage) {
            json = convertJSON5(queryString, outError);
            if (!json)
                return nullptr;
            queryString = json;
        }
        return db->use<C4Query*>([&](C4Database* c4db) {
            return c4query_new2(c4db, (C4QueryLanguage)language, queryString,
                                outErrPos, outError);
        });
    }

    bool valid() const {
        return use<bool>([](C4Query *c4query) {
            return c4query != nullptr;
        });
    }

    const CBLDatabase* database() const {
        return _database;
    }

    alloc_slice explain() const {
        return use<alloc_slice>([](C4Query *c4query) {
            return c4query_explain(c4query);
        });
    }

    unsigned columnCount() const {
        return use<unsigned>([](C4Query *c4query) {
            return c4query_columnCount(c4query);
        });
    }

    slice columnName(unsigned col) const {
        return use<slice>([=](C4Query *c4query) {
            return c4query_columnTitle(c4query, col);
        });
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

    bool setParametersAsJSON(slice json5) {
        alloc_slice json = convertJSON5(json5, nullptr);
        if (!json)
            return false;
        Encoder enc;
        enc.convertJSON(json);
        return _encodeParameters(enc);
    }

    Retained<CBLResultSet> execute(C4Error* outError);

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

    Retained<CBLListenerToken> addChangeListener(CBLQueryChangeListener listener, void *context);

    ListenerToken<CBLQueryChangeListener>* getChangeListener(CBLListenerToken *token) const {
        return _listeners.find(token);
    }

private:
    bool _encodeParameters(Encoder &enc) {
        alloc_slice encodedParameters = enc.finish();
        if (!encodedParameters)
            return false;
        _parameters = encodedParameters;
        use([&](C4Query *c4query) {
            c4query_setParameters(c4query, encodedParameters);
        });
        return true;
    }

    RetainedConst<CBLDatabase>          _database;          // Owning database
    alloc_slice                         _parameters;        // Fleece-encoded param values
    mutable optional<ColumnNamesMap>    _columnNames;       // Maps colum name to index
    mutable once_flag                   _onceColumnNames;   // For lazy init of _columnNames
    Listeners<CBLQueryChangeListener>   _listeners;         // Query listeners
    // Note: Where's the C4Query? It's owned by the base class shared_access_lock.
};


#pragma mark - RESULT SET CLASS:


class CBLResultSet : public CBLRefCounted {
public:
    CBLResultSet(CBLQuery* query, C4QueryEnumerator* qe _cbl_nonnull)
    :_query(query)
    ,_enum(qe)
    { }

    bool next() {
        _asArray = nullptr;
        _asDict = nullptr;
        C4Error error;
        bool more = c4queryenum_next(_enum, &error);
        if (!more && error.code != 0)
            C4LogToAt(kC4QueryLog, kC4LogWarning,
                      "cbl_result_next: got error %d/%d", error.domain, error.code);
        return more;
    }

    Value property(slice prop) const {
        int col = _query->columnNamed(prop);
        return (col >= 0) ? column(col) : nullptr;
    }

    Value column(unsigned col) const {
        if (col < 64 && (_enum->missingColumns & (1ULL<<col)))
            return nullptr;
        return FLArrayIterator_GetValueAt(&_enum->columns, uint32_t(col));
    }

    Array asArray() const {
        if (!_asArray) {
            auto array = MutableArray::newArray();
            unsigned nCols = _query->columnCount();
            array.resize(uint32_t(nCols));
            for (unsigned i = 0; i < nCols; ++i)
                array[i] = column(i);
            _asArray = array;
        }
        return _asArray;
    }

    Dict asDict() const {
        if (!_asDict) {
            auto dict = MutableDict::newDict();
            unsigned nCols = _query->columnCount();
            for (unsigned i = 0; i < nCols; ++i) {
                slice key = _query->columnName(i);
                dict[key] = column(i);
            }
            _asDict = dict;
        }
        return _asDict;
    }

    CBLQuery* query() const {
        return _query;
    }

private:
    Retained<CBLQuery> const         _query;    // The query
    c4::ref<C4QueryEnumerator> const _enum;     // The query enumerator
    mutable MutableArray             _asArray;  // Column values as a Fleece Array
    mutable MutableDict              _asDict;   // Column names/values as a Fleece Dict
};


#pragma mark - QUERY LISTENER:


namespace cbl_internal {

    // Custom subclass of CBLListenerToken for query listeners.
    // (It implements the ListenerToken<> template so that it will work with Listeners<>.)
    template<>
    class ListenerToken<CBLQueryChangeListener> : public CBLListenerToken {
    public:
        ListenerToken(CBLQuery *query,
                      CBLQueryChangeListener callback,
                      void *context)
        :CBLListenerToken((const void*)callback, context)
        ,_query(query)
        {
            query->use([&](C4Query *c4query) {
                _c4obs = c4queryobs_create(c4query,
                                           [](C4QueryObserver*, C4Query*, void *context)
                                                { ((ListenerToken*)context)->queryChanged(); },
                                           this);
            });
        }

        ~ListenerToken() {
            c4queryobs_free(_c4obs);
        }

        void setEnabled(bool enabled) {
            _query->use([&](C4Query *c4query) {
                c4queryobs_setEnabled(_c4obs, enabled);
            });
        }

        CBLQueryChangeListener callback() const           {return (CBLQueryChangeListener)_callback.load();}

        void call() {
            CBLQueryChangeListener cb = callback();
            if (cb)
                cb(_context, _query);
        }

        Retained<CBLResultSet> resultSet(CBLError *error) {
            auto e = c4queryobs_getEnumerator(_c4obs, false, internal(error));
            return e ? new CBLResultSet(_query, e) : nullptr;
        }

    private:
        void queryChanged() {
            _query->database()->notify(this);
        }

        Retained<CBLQuery>  _query;
        C4QueryObserver*    _c4obs {nullptr};
    };

}
