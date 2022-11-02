//
//  Internal.hh
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
#include "CBLBase.h"
#include "CBLPlatform.h"
#include "c4Base.hh"
#include "fleece/slice.hh"
#include "fleece/Fleece.hh"
#include "fleece/RefCounted.hh"
#include "fleece/InstanceCounted.hh"

CBL_ASSUME_NONNULL_BEGIN

struct CBLRefCounted : public fleece::RefCounted, fleece::InstanceCountedIn<CBLRefCounted> {
protected:
    using Value = fleece::Value;
    using Dict = fleece::Dict;
    using Array = fleece::Array;
    using Doc = fleece::Doc;
    using Encoder = fleece::Encoder;
    using MutableDict = fleece::MutableDict;
    using alloc_slice = fleece::alloc_slice;
    using slice = fleece::slice;

    template <class T> using Retained = fleece::Retained<T>;
    template <class T> using RetainedConst = fleece::RetainedConst<T>;
};


/** CBLStoppable objects can be registered / unregisted to / from the database. The registered object will be called
    to stop() when the database is closed. The object will be unregistered either after the object is called to stop() or
    when there is an API call to unregister the object. The database will also retain the object when the object is registered,
    and will release the object when the object is unregistered to ensure that the object is alive until the object is unregistered. */
struct CBLStoppable {
public:
    CBLStoppable(CBLRefCounted* ref)
    :_ref(ref)
    {}
    
    virtual ~CBLStoppable()                             =default;
    virtual void stop() const                           =0;
    
    void retain()                                       {fleece::retain(_ref);}
    void release()                                      {fleece::release(_ref);}
    
protected:
    CBLRefCounted* _ref;
};


namespace cbl_internal {
    // Converting C4Error <--> CBLError
    static inline       C4Error* _cbl_nullable internal(CBLError* _cbl_nullable error) {return (C4Error*)error;}
    static inline const C4Error& internal(        const CBLError &error) {return (const C4Error&)error;}

    static inline       CBLError* _cbl_nullable external(C4Error* _cbl_nullable error) {return (CBLError*)error;}
    static inline const CBLError& external(        const C4Error &error) {return (const CBLError&)error;}

    template <typename T>
    static inline void writeOptionalKey(fleece::Encoder &enc, const char *propName, T value) {
        if (value)
            enc[fleece::slice(propName)] = value;
    }

    void BridgeException(const char *fnName, CBLError* _cbl_nullable outError) noexcept;

    void BridgeExceptionWarning(const char *fnName) noexcept;

    fleece::alloc_slice convertJSON5(fleece::slice json5);

#ifdef __ANDROID__

    void initContext(CBLInitContext context);

    const CBLInitContext* _cbl_nullable getInitContext() noexcept;

#endif

}

using namespace cbl_internal;


#define catchAndBridgeReturning(OUTERROR, VALUE) \
    catch (...) { cbl_internal::BridgeException(__FUNCTION__, OUTERROR); return VALUE; }

#define catchAndBridge(OUTERROR) \
    catchAndBridgeReturning(OUTERROR, {})

#define catchAndWarn() \
    catchAndBridge(nullptr)


#define LOCK(MUTEX)     std::lock_guard<decltype(MUTEX)> _lock(MUTEX)

CBL_ASSUME_NONNULL_END
