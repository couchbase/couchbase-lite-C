//
// CBLEncryptable_Internal.hh
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

#pragma once
#include "CBLEncryptable.h"
#include "CBLDocument_Internal.hh"

#ifdef COUCHBASE_ENTERPRISE

CBL_ASSUME_NONNULL_BEGIN

struct CBLEncryptable : public CBLRefCounted {
public:
    CBLEncryptable(FLDict properties) : _properties(Dict(properties)) {
        assert_precondition(properties);
        assert_precondition(isEncryptableValue(properties));
    }
    
    Dict properties() const {
        return _properties.asDict();
    }
    
    FLValue value() const {
        return properties()[kCBLEncryptableValueProperty];
    }
    
    static bool isEncryptableValue(FLDict _cbl_nullable dict) {
        FLValue cbltype= FLDict_Get(dict, C4Document::kObjectTypeProperty);
        return cbltype && slice(FLValue_AsString(cbltype)) == C4Document::kObjectType_Encryptable;
    }
    
    static CBLEncryptable* _cbl_nullable getEncryptableValue(FLDict _cbl_nullable dict) {
        if (!dict)
            return nullptr;
        
        auto doc = CBLDocument::containing(Dict(dict));
        if (!doc)
            return nullptr;
        return doc->getEncryptableValue(dict);
    }
    
    static Retained<CBLEncryptable> createWithNull() {
        auto dict = createDict();
        FLSlot_SetNull(FLMutableDict_Set(dict, kCBLEncryptableValueProperty));
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithBool(bool value) {
        auto dict = createDict();
        FLSlot_SetBool(FLMutableDict_Set(dict, kCBLEncryptableValueProperty), value);
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithInt(int64_t value) {
        auto dict = createDict();
        FLSlot_SetInt(FLMutableDict_Set(dict, kCBLEncryptableValueProperty), value);
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithUInt(uint64_t value) {
        auto dict = createDict();
        FLSlot_SetUInt(FLMutableDict_Set(dict, kCBLEncryptableValueProperty), value);
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithFloat(float value) {
        auto dict = createDict();
        FLSlot_SetFloat(FLMutableDict_Set(dict, kCBLEncryptableValueProperty), value);
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithDouble(double value) {
        auto dict = createDict();
        FLSlot_SetDouble(FLMutableDict_Set(dict, kCBLEncryptableValueProperty), value);
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithString(FLString value) {
        auto dict = createDict();
        FLSlot_SetString(FLMutableDict_Set(dict, kCBLEncryptableValueProperty), value);
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithValue(FLValue value) {
        auto dict = createDict();
        FLSlot_SetValue(FLMutableDict_Set(dict, kCBLEncryptableValueProperty), value);
        return new CBLEncryptable(dict);
    }
    
    static Retained<CBLEncryptable> createWithArray(FLArray value) {
        return createWithValue((FLValue)value);
    }
    
    static Retained<CBLEncryptable> createWithDict(FLDict value) {
        return createWithValue((FLValue)value);
    }

private:
    static FLMutableDict createDict() {
        auto dict = FLMutableDict_New();
        FLSlot_SetString(FLMutableDict_Set(dict, kCBLTypeProperty), kCBLEncryptableType);
        return dict;
    }
    
    fleece::RetainedValue const         _properties;
};

CBL_ASSUME_NONNULL_END

#endif
