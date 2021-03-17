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


const FLString kCBLAuthDefaultCookieName = FLSTR("SyncGatewaySession");


CBLEndpoint* CBLEndpoint_NewWithURL(FLString url) CBLAPI {
    return new CBLURLEndpoint(url);
}

#ifdef COUCHBASE_ENTERPRISE
CBLEndpoint* CBLEndpoint_NewWithLocalDB(CBLDatabase* db) CBLAPI {
    return new CBLLocalEndpoint(db);
}
#endif

void CBLEndpoint_Free(CBLEndpoint *endpoint) CBLAPI {
    delete endpoint;
}

CBLAuthenticator* CBLAuth_NewPassword(FLString username, FLString password) CBLAPI {
    return new BasicAuthenticator(username, password);
}

CBLAuthenticator* CBLAuth_NewSession(FLString sessionID, FLString cookieName) CBLAPI {
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

void CBLReplicator_Start(CBLReplicator* repl, bool reset) CBLAPI{repl->start(reset);}
void CBLReplicator_Stop(CBLReplicator* repl) CBLAPI             {repl->stop();}
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
    return retain(repl->addChangeListener(listener, context));
}

CBLListenerToken* CBLReplicator_AddDocumentReplicationListener(CBLReplicator* repl,
                                                    CBLDocumentReplicationListener listener,
                                                    void *context) CBLAPI
{
    return retain(repl->addDocumentListener(listener, context));
}
