//
// ContextManager.cc
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

#include "ContextManager.hh"

using namespace std;
using namespace fleece;

namespace cbl_internal {

    ContextManager &ContextManager::shared() {
        static ContextManager* sContextManager = new ContextManager();
        return *sContextManager;
    }

    ContextManager::ContextManager() { }

    void* ContextManager::registerObject(CBLRefCounted* object) {
        std::lock_guard<std::mutex> lock(_mutex);
        _contexts.insert({(void*)object, retained(object)});
        return object;
    }

    void ContextManager::unregisterObject(void* ptr) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (auto i = _contexts.find(ptr); i != _contexts.end()) {
            _contexts.erase(i);
        }
    }

    Retained<CBLRefCounted> ContextManager::getObject(void* ptr) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (auto i = _contexts.find(ptr); i != _contexts.end()) {
            return i->second;
        }
        return nullptr;
    }

}
