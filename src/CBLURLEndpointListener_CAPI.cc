//
// CBLURLEndoiubtListeber_CAPI.cc
//
// Copyright © 2025 Couchbase. All rights reserved.
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

#ifdef COUCHBASE_ENTERPRISE

#include "CBLURLEndpointListener_Internal.hh"

CBLListenerAuthenticator* CBLListenerAuth_CreatePassword(CBLListenerPasswordAuthCallback auth) noexcept {
    try {
        return new CBLListenerAuthenticator(auth);
    } catchAndWarn();
}

CBLListenerAuthenticator* CBLListenerAuth_CreateCertificate(CBLListenerCertAuthCallback auth) noexcept {
    try {
        return new CBLListenerAuthenticator(auth);
    } catchAndWarn();
}

CBLListenerAuthenticator* CBLListenerAuth_CreateCertificateWithRootCerts(CBLCert* rootCerts) noexcept {
    try {
        return new CBLListenerAuthenticator(rootCerts);
    } catchAndWarn();
}

void CBLListenerAuth_Free(CBLListenerAuthenticator* _cbl_nullable auth) noexcept {
    if (auth) delete auth;
}

CBLURLEndpointListener* CBLURLEndpointListener_Create(const CBLURLEndpointListenerConfiguration* conf, CBLError* outError) noexcept {
    try {
        return retain(new CBLURLEndpointListener(*conf));
    } catchAndBridge(outError);
}

const CBLURLEndpointListenerConfiguration* CBLURLEndpointListener_Config(const CBLURLEndpointListener* listener) noexcept {
    return listener->configuration();
}

uint16_t CBLURLEndpointListener_Port(const CBLURLEndpointListener* listener) noexcept {
    return listener->port();
}

FLMutableArray CBLURLEndpointListener_Urls(const CBLURLEndpointListener* listener) noexcept {
    try {
        return (FLMutableArray)FLValue_Retain(listener->getUrls());
    } catchAndWarn()
}

CBLConnectionStatus CBLURLEndpointListener_Status(const CBLURLEndpointListener* listener) noexcept {
    return listener->getConnectionStatus();
}

bool CBLURLEndpointListener_Start(CBLURLEndpointListener* listener, CBLError* outError) noexcept {
    try {
        listener->start();
        return true;
    } catchAndBridge(outError)
}

void CBLURLEndpointListener_Stop(CBLURLEndpointListener* listener) noexcept {
    try {
        listener->stop();
    } catchAndWarnNoReturn()
}

#endif
