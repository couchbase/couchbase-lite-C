//
// ContextManager.hh
//
// Copyright © 2024 Couchbase. All rights reserved.
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
#include "Internal.hh"
#include <mutex>
#include <unordered_map>

CBL_ASSUME_NONNULL_BEGIN

namespace cbl_internal {

    /**
     Thread-safe context manager for retaining and mapping the object with its pointer value which can be used
     as a (captured) context for LiteCore's callback which could be either in C++ or C. This would allow the
     callback to verify that the context pointer value is still valid or not before using it. The implementation simply
     stores the object in a map by using its memory address as the key and returns the memory address as
     the pointer value.
      
     @note
     There is a chance that a new registered objects may have the same memory address as the ones previously
     unregistered. This implementation can be improved to reduce a chance of reusing the same memory address
     by generating integer keys with reuseable integer number + cycle count as inspired by the implementation of
     C# GCHandle. For now, the context object MUST BE validated for its originality before use. */
    class ContextManager {
    public:
        static ContextManager& shared();
        
        void* registerObject(CBLRefCounted* object);
        
        void unregisterObject(void* ptr);
        
        fleece::Retained<CBLRefCounted> getObject(void* ptr);
        
    private:
        ContextManager();
        
        std::mutex _mutex;
        std::unordered_map<void*, fleece::Retained<CBLRefCounted>> _contexts;
    };

}

CBL_ASSUME_NONNULL_END
