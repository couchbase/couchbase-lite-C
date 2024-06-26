//
// CBLQueryIndex_CAPI.cc
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

#include "CBLQueryIndex.h"
#include "CBLQueryIndex_Internal.hh"

FLString CBLQueryIndex_Name(const CBLQueryIndex* index) noexcept {
    try {
        return index->name();
    } catchAndWarn()
}

CBLCollection* CBLQueryIndex_Collection(const CBLQueryIndex* index) noexcept {
    try {
        return index->collection();
    } catchAndWarn()
}

#ifdef COUCHBASE_ENTERPRISE

CBLIndexUpdater* _cbl_nullable CBLQueryIndex_BeginUpdate(CBLQueryIndex* index, size_t limit,
                                                    CBLError* _cbl_nullable outError) noexcept
{
    try {
        auto updater = index->beginUpdate(limit);
        return updater ? std::move(updater).detach() : nullptr;
    } catchAndBridge(outError)
}

size_t CBLIndexUpdater_Count(const CBLIndexUpdater* updater) noexcept {
    try {
        return updater->count();
    } catchAndAbort()
}

FLValue CBLIndexUpdater_Value(CBLIndexUpdater* updater, size_t index) noexcept {
    try {
        return updater->value(index);
    } catchAndAbort()
}

bool CBLIndexUpdater_SetVector(CBLIndexUpdater* updater, size_t index, const float vector[_cbl_nullable],
                               size_t dimension, CBLError* _cbl_nullable outError) noexcept
{
    try {
        updater->setVector(index, vector, dimension);
        return true;
    } catchAndBridge(outError)
}

void CBLIndexUpdater_SkipVector(CBLIndexUpdater* updater, size_t index) noexcept {
    try {
        updater->skipVector(index);
    } catchAndAbort()
}

bool CBLIndexUpdater_Finish(CBLIndexUpdater* updater, CBLError* _cbl_nullable outError) noexcept {
    try {
        updater->finish();
        return true;
    } catchAndBridge(outError)
}

#endif
