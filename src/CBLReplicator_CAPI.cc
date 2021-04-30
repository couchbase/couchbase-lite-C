//
// CBLReplicator_CAPI.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "CBLReplicator.h"
#include "CBLReplicator_Internal.hh"


const FLString kCBLAuthDefaultCookieName = FLSTR("SyncGatewaySession");

CBLReplicatorConfiguration CBLReplicatorConfiguration_Default() noexcept {
    return CBLReplicator::defaultConfiguration();
}

CBLEndpoint* CBLEndpoint_NewWithURL(FLString url) noexcept {
    try {
        return new CBLURLEndpoint(url);
    } catchAndWarn()
}

#ifdef COUCHBASE_ENTERPRISE
CBLEndpoint* CBLEndpoint_NewWithLocalDB(CBLDatabase* db) noexcept {
    try {
        return new CBLLocalEndpoint(db);
    } catchAndWarn()
}
#endif

void CBLEndpoint_Free(CBLEndpoint *endpoint) noexcept {
    delete endpoint;
}

CBLAuthenticator* CBLAuth_NewPassword(FLString username, FLString password) noexcept {
    try {
        return new BasicAuthenticator(username, password);
    } catchAndWarn()
}

CBLAuthenticator* CBLAuth_NewSession(FLString sessionID, FLString cookieName) noexcept {
    try {
        return new SessionAuthenticator(sessionID, cookieName);
    } catchAndWarn()
}

void CBLAuth_Free(CBLAuthenticator *auth) noexcept {
    delete auth;
}

CBLReplicator* CBLReplicator_New(const CBLReplicatorConfiguration* conf, CBLError *outError) noexcept {
    try {
        return retain(new CBLReplicator(*conf));
    } catchAndBridge(outError)
}

const CBLReplicatorConfiguration* CBLReplicator_Config(CBLReplicator* repl) noexcept {
    return repl->configuration();
}

CBLReplicatorStatus CBLReplicator_Status(CBLReplicator* repl) noexcept {
    return repl->status();
}

void CBLReplicator_Start(CBLReplicator* repl, bool reset) noexcept        {repl->start(reset);}
void CBLReplicator_Stop(CBLReplicator* repl) noexcept                     {repl->stop();}
void CBLReplicator_SetHostReachable(CBLReplicator* repl, bool r) noexcept {repl->setHostReachable(r);}
void CBLReplicator_SetSuspended(CBLReplicator* repl, bool sus) noexcept   {repl->setSuspended(sus);}

FLDict CBLReplicator_PendingDocumentIDs(CBLReplicator *repl, CBLError *outError) noexcept {
    try {
        auto result = FLDict_Retain(repl->pendingDocumentIDs());
        if (!result)
            if (outError) outError->code = 0;
        return result;
    } catchAndBridge(outError)
}

bool CBLReplicator_IsDocumentPending(CBLReplicator *repl, FLString docID, CBLError *outError) noexcept {
    try {
        bool result = repl->isDocumentPending(docID);
        if (!result)
            if (outError) outError->code = 0;
        return result;
    } catchAndBridge(outError)
}

CBLListenerToken* CBLReplicator_AddChangeListener(CBLReplicator* repl,
                                                  CBLReplicatorChangeListener listener,
                                                  void *context) noexcept
{
    return retain(repl->addChangeListener(listener, context));
}

CBLListenerToken* CBLReplicator_AddDocumentReplicationListener(CBLReplicator* repl,
                                                    CBLDocumentReplicationListener listener,
                                                    void *context) noexcept
{
    return retain(repl->addDocumentListener(listener, context));
}
