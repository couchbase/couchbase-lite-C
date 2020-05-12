//
// CBLReplicator.cc
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


const char* const kCBLAuthDefaultCookieName = "SyncGatewaySession";


CBLEndpoint* CBLEndpoint_NewWithURL(const char *url _cbl_nonnull) CBLAPI {
    return CBLEndpoint_NewWithURL_s(slice(url));
}

CBLEndpoint* CBLEndpoint_NewWithURL_s(FLString url) CBLAPI {
    return new CBLURLEndpoint(url);
}

#ifdef COUCHBASE_ENTERPRISE
CBLEndpoint* CBLEndpoint_NewWithLocalDB(CBLDatabase* db) CBLAPI {
    return new CBLLocalEndpoint(db);
}
#else
// Placeholder function for Xcode, since it has _CBLEndpoint_NewWithLocalDB in CBL.exp
extern "C" CBLEndpoint* CBLEndpoint_NewWithLocalDB(CBLDatabase* db) CBLAPI;
CBLEndpoint* CBLEndpoint_NewWithLocalDB(CBLDatabase* db) CBLAPI { abort(); }
#endif

void CBLEndpoint_Free(CBLEndpoint *endpoint) CBLAPI {
    delete endpoint;
}

CBLAuthenticator* CBLAuth_NewBasic(const char *username, const char *password) CBLAPI {
    return CBLAuth_NewBasic_s(slice(username), slice(password));
}

CBLAuthenticator* CBLAuth_NewBasic_s(FLString username, FLString password) CBLAPI {
    return new BasicAuthenticator(username, password);
}

CBLAuthenticator* CBLAuth_NewSession(const char *sessionID, const char *cookieName) CBLAPI {
    return CBLAuth_NewSession_s(slice(sessionID), slice(cookieName));
}

CBLAuthenticator* CBLAuth_NewSession_s(FLString sessionID, FLString cookieName) CBLAPI {
    return new SessionAuthenticator(sessionID, cookieName);
}

void CBLAuth_Free(CBLAuthenticator *auth) CBLAPI {
    delete auth;
}

CBLReplicator* CBLReplicator_New(const CBLReplicatorConfiguration* conf, CBLError *outError) CBLAPI {
    return validated(new CBLReplicator(conf), outError);
}

const CBLReplicatorConfiguration* CBLReplicator_Config(CBLReplicator* repl) CBLAPI {
    return repl->configuration();
}

CBLReplicatorStatus CBLReplicator_Status(CBLReplicator* repl) CBLAPI {
    return repl->status();
}

void CBLReplicator_Start(CBLReplicator* repl) CBLAPI            {repl->start();}
void CBLReplicator_Stop(CBLReplicator* repl) CBLAPI             {repl->stop();}
void CBLReplicator_ResetCheckpoint(CBLReplicator* repl) CBLAPI  {repl->resetCheckpoint();}
void CBLReplicator_SetHostReachable(CBLReplicator* repl, bool r) CBLAPI {repl->setHostReachable(r);}
void CBLReplicator_SetSuspended(CBLReplicator* repl, bool sus) CBLAPI   {repl->setSuspended(sus);}

FLDict CBLReplicator_PendingDocumentIDs(CBLReplicator *repl, CBLError *outError) CBLAPI {
    return FLDict_Retain(repl->pendingDocumentIDs(outError));
}

bool CBLReplicator_IsDocumentPending(CBLReplicator *repl, FLString docID, CBLError *error) CBLAPI {
    return repl->isDocumentPending(docID, error);
}

CBLListenerToken* CBLReplicator_AddChangeListener(CBLReplicator* repl,
                                                  CBLReplicatorChangeListener listener,
                                                  void *context) CBLAPI
{
    return repl->addChangeListener(listener, context);
}

CBLListenerToken* CBLReplicator_AddDocumentListener(CBLReplicator* repl,
                                                    CBLReplicatedDocumentListener listener,
                                                    void *context) CBLAPI
{
    return repl->addDocumentListener(listener, context);
}
