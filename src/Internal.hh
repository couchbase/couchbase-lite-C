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
#include "InstanceCounted.hh"
#include <memory>
#include <string>
#include <vector>


struct CBLRefCounted : public fleece::RefCounted, fleece::InstanceCountedIn<CBLRefCounted> {
};


namespace cbl_internal {
    static inline C4Error* internal(CBLError *error)             {return (C4Error*)error;}
    static inline const C4Error* internal(const CBLError *error) {return (const C4Error*)error;}
    static inline const CBLError* external(const C4Error *error) {return (const CBLError*)error;}

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
