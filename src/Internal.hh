//
//  Internal.hh
//  CBL_C
//
//  Created by Jens Alfke on 12/20/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "c4.h"
#include "fleece/slice.hh"
#include "fleece/Fleece.hh"
#include "RefCounted.hh"
#include <string>


struct CBLRefCounted : public fleece::RefCounted {
};


struct CBLDatabase : public CBLRefCounted {
    C4Database* const c4db;
    std::string const name;
    std::string const path;

    CBLDatabase(C4Database* _cblnonnull db, const std::string &name_, const std::string &path_)
    :c4db(db)
    ,name(name_)
    ,path(path_)
    { }

    virtual ~CBLDatabase() {
        c4db_free(c4db);
    }
};


namespace cbl_internal {
    static inline const C4Error* internal(const CBLError *error) {return (const C4Error*)error;}
    static inline C4Error* internal(CBLError *error)       {return (C4Error*)error;}
    static inline C4Database* internal(const CBLDatabase *db)    {return db->c4db;}

    template <typename T>
    T* validated(T *obj, CBLError *outError) {
        if (obj->validate(outError))
            return obj;
        delete obj;
        return nullptr;
    }

    template <typename T>
    static inline void writeOptionalKey(fleece::Encoder &enc, const char *propName, T value) {
        if (value)
            enc[fleece::slice(propName)] = value;
    }
}

using namespace cbl_internal;
