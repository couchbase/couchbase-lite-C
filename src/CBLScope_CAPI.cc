//
// CBLCollection_CAPI.cc
//
// Copyright (C) 2022 Couchbase, Inc All rights reserved.
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

#include "CBLScope_Internal.hh"
#include "CBLCollection_Internal.hh"

#pragma mark - CONSTANTS

const FLString kCBLDefaultScopeName = FLSTR("_default");

#pragma mark - ACCESSORS

FLString CBLScope_Name(const CBLScope* scope) noexcept {
    return scope->name();
}

CBLDatabase* CBLScope_Database(const CBLScope* scope) noexcept {
    return scope->database();
}

#pragma mark - COLLECTIONS

FLMutableArray CBLScope_CollectionNames(const CBLScope* scope,
                                        CBLError* outError) noexcept
{
    try {
        return scope->collectionNames();
    } catchAndBridge(outError)
}

CBLCollection* CBLScope_Collection(const CBLScope* scope,
                                   FLString collectionName,
                                   CBLError* outError) noexcept
{
    try {
        return scope->getCollection(collectionName).detach();
    } catchAndBridge(outError)
}
