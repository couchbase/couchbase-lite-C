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


static inline C4Error* internal(CBLError *error)       {return (C4Error*)error;}
static inline C4Database* internal(CBLDatabase *db)    {return db->c4db;}
