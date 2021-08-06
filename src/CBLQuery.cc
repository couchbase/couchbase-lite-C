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

#include "CBLDatabase_Internal.hh"
#include "CBLQuery_Internal.hh"

void ListenerToken<CBLQueryChangeListener>::setEnabled(bool enabled) {
    auto c4query = _query->_c4query.useLocked();
    CBLDatabase* db = const_cast<CBLDatabase*>(_query->database());
    if (enabled) {
        if (!db->registerStoppable(this)) {
            CBL_Log(kCBLLogDomainQuery, kCBLLogWarning,
                    "Couldn't enable the Query Listener as the database is closing or closed.");
            return;
        }
    }
    _c4obs->setEnabled(enabled);
    if (!enabled)
        db->unregisterStoppable(this);
}
