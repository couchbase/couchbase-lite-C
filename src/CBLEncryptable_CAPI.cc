//
// CBLEncryptable_CAPI.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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

#include "CBLEncryptable_Internal.hh"

#ifdef COUCHBASE_ENTERPRISE

using namespace fleece;

CBL_CORE_API const FLSlice kCBLEncryptableType            = C4Document::kObjectType_Encryptable;
CBL_CORE_API const FLSlice kCBLEncryptableValueProperty   = C4Document::kValueToEncryptProperty;

CBLEncryptable* CBLEncryptable_CreateWithNull() noexcept {
    return CBLEncryptable::createWithNull().detach();
}

CBLEncryptable* CBLEncryptable_CreateWithBool(bool value) noexcept {
    return CBLEncryptable::createWithBool(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithInt(int64_t value) noexcept {
    return CBLEncryptable::createWithInt(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithUInt(uint64_t value) noexcept {
    return CBLEncryptable::createWithUInt(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithFloat(float value) noexcept {
    return CBLEncryptable::createWithFloat(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithDouble(double value) noexcept {
    return CBLEncryptable::createWithDouble(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithString(FLString value) noexcept {
    return CBLEncryptable::createWithString(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithValue(FLValue value) noexcept {
    return CBLEncryptable::createWithValue(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithArray(FLArray value) noexcept {
    return CBLEncryptable::createWithArray(value).detach();
}

CBLEncryptable* CBLEncryptable_CreateWithDict(FLDict value) noexcept {
    return CBLEncryptable::createWithDict(value).detach();
}

FLValue CBLEncryptable_Value(const CBLEncryptable* encryptable) noexcept {
    return encryptable->value();
}

FLDict CBLEncryptable_Properties(const CBLEncryptable* encryptable) noexcept {
    return encryptable->properties();
}

bool FLDict_IsEncryptableValue(FLDict _cbl_nullable dict) noexcept {
    return CBLEncryptable::isEncryptableValue(dict);
}

const CBLEncryptable* FLDict_GetEncryptableValue(FLDict _cbl_nullable dict) noexcept {
    return CBLEncryptable::getEncryptableValue(dict);
}

void FLSlot_SetEncryptableValue(FLSlot slot, const CBLEncryptable* encryptable) noexcept {
    Dict props = encryptable->properties();
    MutableDict mProps = props.asMutable();
    if (!mProps)
        mProps = props.mutableCopy();
    FLSlot_SetValue(slot, mProps);
}

#endif
