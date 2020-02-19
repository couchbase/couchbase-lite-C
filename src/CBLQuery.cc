//
// CBLQuery.cc
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
#include <unordered_map>

using namespace std;
using namespace fleece;


#pragma mark - QUERY CLASS:


class CBLQuery : public CBLRefCounted, public litecore::shared_access_lock<C4Query*> {
public:

    CBLQuery(const CBLDatabase* db _cbl_nonnull,
             CBLQueryLanguage language,
             const char *queryCString _cbl_nonnull,
             int *outErrPos,
             C4Error* outError)
    :shared_access_lock<C4Query*>(create(db, language, queryCString, outErrPos, outError), db)
    ,_database(db)
    { }

    ~CBLQuery() {
        use([](C4Query *c4query) {
            c4query_release(c4query);
        });
    }

    static C4Query* create(const CBLDatabase* db _cbl_nonnull,
                           CBLQueryLanguage language,
                           const char *queryCString _cbl_nonnull,
                           int *outErrPos,
                           C4Error* outError)
    {
        slice queryString;
        alloc_slice json;
        if (language == kCBLJSONLanguage) {
            json = convertJSON5(queryCString, outError);
            if (!json)
                return nullptr;
            queryString = json;
        } else {
            queryString = slice(queryCString);
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

    void setParameters(Dict parameters) {
        Encoder enc;
        enc.writeValue(parameters);
        _encodeParameters(enc);
    }

    bool setParametersAsJSON(const char* json5) {
        alloc_slice json = convertJSON5(json5, nullptr);
        if (!json)
            return false;
        Encoder enc;
        enc.convertJSON(json);
        return _encodeParameters(enc);
    }

    Retained<CBLResultSet> execute(C4Error* outError);

    int columnNamed(slice name) {
        call_once(_onceColumnNames, [this]{
            _columnNames.reset(new std::unordered_map<slice, uint32_t>);
            unsigned nCols = columnCount();
            _columnNames->reserve(nCols);
            for (unsigned col = 0; col < nCols; ++col)
                _columnNames->insert({columnName(col), col});
        });
        auto i = _columnNames->find(name);
        return (i != _columnNames->end()) ? i->second : -1;
    }

    Dict parameters() {
        if (!_parameters)
            return nullptr;
        return Value::fromData(_parameters, kFLTrusted).asDict();
    }

    CBLListenerToken* addChangeListener(CBLQueryChangeListener listener, void *context);

    ListenerToken<CBLQueryChangeListener>* getChangeListener(CBLListenerToken *token) {
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

    RetainedConst<CBLDatabase> _database;
    alloc_slice _parameters;
    unique_ptr<std::unordered_map<slice, unsigned>> _columnNames;
    once_flag _onceColumnNames;
    Listeners<CBLQueryChangeListener> _listeners;
};


#pragma mark - RESULT SET CLASS:


class CBLResultSet : public CBLRefCounted {
public:
    CBLResultSet(CBLQuery* query, C4QueryEnumerator* qe _cbl_nonnull)
    :_query(query)
    ,_enum(qe)
    { }

    bool next() {
        C4Error error;
        bool more = c4queryenum_next(_enum, &error);
        if (!more && error.code != 0)
            C4LogToAt(kC4QueryLog, kC4LogWarning,
                      "cbl_result_next: got error %d/%d", error.domain, error.code);
        return more;
    }

    Value property(const char *prop) {
        int col = _query->columnNamed(slice(prop));
        return (col >= 0) ? column(col) : nullptr;
    }

    Value column(unsigned col) {
        if (col < 64 && (_enum->missingColumns & (1ULL<<col)))
            return nullptr;
        return FLArrayIterator_GetValueAt(&_enum->columns, uint32_t(col));
    }

private:
    Retained<CBLQuery> const _query;
    c4::ref<C4QueryEnumerator> const _enum;
};


Retained<CBLResultSet> CBLQuery::execute(C4Error* outError) {
    auto qe = use<C4QueryEnumerator*>([=](C4Query *c4query) {
        return c4query_run(c4query, nullptr, nullslice, outError);
    });
    return qe ? retained(new CBLResultSet(this, qe)) : nullptr;
}


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

        CBLQueryChangeListener callback() const           {return (CBLQueryChangeListener)_callback.load();}

        void call() {
            CBLQueryChangeListener cb = callback();
            if (cb)
                cb(_context, _query);
        }

        CBLResultSet* resultSet(CBLError *error) {
            auto e = c4queryobs_getEnumerator(_c4obs, internal(error));
            _resultSet = e ? new CBLResultSet(_query, c4queryenum_retain(e)) : nullptr;
            return _resultSet;
        }

    private:
        void queryChanged() {
            _query->database()->notify(this);
        }

        Retained<CBLQuery> _query;
        C4QueryObserver* _c4obs {nullptr};
        Retained<CBLResultSet> _resultSet;
    };

}


CBLListenerToken* CBLQuery::addChangeListener(CBLQueryChangeListener listener, void *context) {
    auto token = new ListenerToken<CBLQueryChangeListener>(this, listener, context);
    _listeners.add(token);
    return token;
}


#pragma mark - PUBLIC API:


CBLQuery* CBLQuery_New(const CBLDatabase* db _cbl_nonnull,
                       CBLQueryLanguage language,
                       const char *queryString _cbl_nonnull,
                       int *outErrorPos,
                       CBLError* outError) CBLAPI
{
    auto query = retained(new CBLQuery(db, language, queryString, outErrorPos, internal(outError)));
    return query->valid() ? retain(query.get()) : nullptr;
}

FLDict CBLQuery_Parameters(CBLQuery* _cbl_nonnull query) CBLAPI {
    return query->parameters();
}

void CBLQuery_SetParameters(CBLQuery* query _cbl_nonnull, FLDict parameters) CBLAPI {
    query->setParameters(parameters);
}

bool CBLQuery_SetParametersAsJSON(CBLQuery* query, const char* json5) CBLAPI {
    query->setParametersAsJSON(json5);
    return true;
}

CBLResultSet* CBLQuery_Execute(CBLQuery* query _cbl_nonnull, CBLError* outError) CBLAPI {
    return retain(query->execute(internal(outError)).get());
}

FLSliceResult CBLQuery_Explain(CBLQuery* query _cbl_nonnull) CBLAPI {
    return FLSliceResult(query->explain());
}

unsigned CBLQuery_ColumnCount(CBLQuery* query _cbl_nonnull) CBLAPI {
    return query->columnCount();
}

FLSlice CBLQuery_ColumnName(CBLQuery* query _cbl_nonnull, unsigned col) CBLAPI {
    return query->columnName(col);
}

CBLListenerToken* CBLQuery_AddChangeListener(CBLQuery* query _cbl_nonnull,
                                             CBLQueryChangeListener listener _cbl_nonnull,
                                             void *context) CBLAPI
{
    return query->addChangeListener(listener, context);
}

CBLResultSet* CBLQuery_CurrentResults(CBLQuery* query,
                                      CBLListenerToken *token,
                                      CBLError *outError) CBLAPI
{
    auto listener = query->getChangeListener(token);
    if (!listener) {
        setError(internal(outError), LiteCoreDomain, kC4ErrorNotFound,
                 "Listener token is not valid for this query"_sl);
        return nullptr;
    }
    return listener->resultSet(outError);
}

bool CBLResultSet_Next(CBLResultSet* rs _cbl_nonnull) CBLAPI {
    return rs->next();
}

FLValue CBLResultSet_ValueForKey(CBLResultSet* rs _cbl_nonnull, const char *property) CBLAPI {
    return rs->property(property);
}

FLValue CBLResultSet_ValueAtIndex(CBLResultSet* rs _cbl_nonnull, unsigned column) CBLAPI {
    return rs->column(column);
}


#pragma mark - INDEXES:


bool CBLDatabase_CreateIndex(CBLDatabase *db _cbl_nonnull,
                        const char* name _cbl_nonnull,
                        CBLIndexSpec spec,
                        CBLError *outError) CBLAPI
{
    C4IndexOptions options = {};
    options.language = spec.language;
    options.ignoreDiacritics = spec.ignoreAccents;
    return db->use<bool>([&](C4Database *c4db) {
        return c4db_createIndex(c4db,
                                slice(name),
                                slice(spec.keyExpressionsJSON),
                                (C4IndexType)spec.type,
                                &options,
                                internal(outError));
    });
}

bool CBLDatabase_DeleteIndex(CBLDatabase *db _cbl_nonnull,
                        const char *name _cbl_nonnull,
                        CBLError *outError) CBLAPI
{
    return db->use<bool>([&](C4Database *c4db) {
        return c4db_deleteIndex(c4db, slice(name), internal(outError));
    });
}

FLMutableArray CBLDatabase_IndexNames(CBLDatabase *db _cbl_nonnull) CBLAPI {
    return db->use<FLMutableArray>([&](C4Database *c4db) {
        Doc doc(alloc_slice(c4db_getIndexes(c4db, nullptr)));
        MutableArray indexes = doc.root().asArray().mutableCopy(kFLDeepCopyImmutables);
        return FLMutableArray_Retain(indexes);
    });
}

