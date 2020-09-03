# Couchbase Lite listener token
#
# Copyright (c) 2020 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import CouchbaseLite/private/cbl

{.experimental: "notnil".}


type
  ListenerToken* = object
    ## A reference to a registered listener callback. When this object goes out
    ## of scope, the listener is removed and the callback will no longer be
    ## called.
    handle: CBLListenerToken not nil

proc `=destroy`(t: var ListenerToken) =
  remove(t.handle)

proc `=`(dst: var ListenerToken, src: ListenerToken) {.error.}
