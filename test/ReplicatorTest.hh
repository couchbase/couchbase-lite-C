//
// ReplicatorTest.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "CBLTest_Cpp.hh"
#include "cbl++/CouchbaseLite.hh"
#include <iostream>
#include <thread>
#include <set>
#include <vector>

using namespace std;
using namespace fleece;
using namespace cbl;


class ReplicatorTest : public CBLTest_Cpp {
public:
    CBLReplicatorConfiguration config = CBLReplicatorConfiguration_Default();
    CBLReplicator *repl = nullptr;
    set<string> docsNotified;
    CBLError replError = {};
    bool logEveryDocument = true;

    ReplicatorTest() {
        config.database = db.ref();
        config.replicatorType = kCBLReplicatorTypePull;
        config.context = this;
    }

    void replicate() {
        CBLError error;
        repl = CBLReplicator_New(&config, &error);
        REQUIRE(repl);

        auto ctoken = CBLReplicator_AddChangeListener(repl, [](void *context, CBLReplicator *r,
                                                 const CBLReplicatorStatus *status) {
            ((ReplicatorTest*)context)->statusChanged(r, *status);
        }, this);

        auto dtoken = CBLReplicator_AddDocumentReplicationListener(repl, [](void *context, CBLReplicator *r, bool isPush,
                                                                 unsigned numDocuments,
                                                                 const CBLReplicatedDocument* documents) {
            ((ReplicatorTest*)context)->docProgress(r, isPush, numDocuments, documents);
        }, this);

        CBLReplicator_Start(repl, false);

        cerr << "Waiting...\n";
        CBLReplicatorStatus status;
        while ((status = CBLReplicator_Status(repl)).activity != kCBLReplicatorStopped) {
            this_thread::sleep_for(100ms);
        }
        cerr << "Finished with activity=" << status.activity
             << ", error=(" << status.error.domain << "/" << status.error.code << ")\n";

        CBLListener_Remove(ctoken);
        CBLListener_Remove(dtoken);
    }

    void statusChanged(CBLReplicator *r, const CBLReplicatorStatus &status) {
        CHECK(r == repl);
        cerr << "--- PROGRESS: status=" << status.activity << ", fraction=" << status.progress.fractionComplete << ", err=" << status.error.domain << "/" << status.error.code << "\n";
        if (status.error.code && !replError.code)
            replError = status.error;
    }

    void docProgress(CBLReplicator *r, bool isPush,
                     unsigned numDocuments,
                     const CBLReplicatedDocument* documents) {
        CHECK(r == repl);
        cerr << "--- " << numDocuments << " docs " << (isPush ? "pushed" : "pulled") << ":";
        if (logEveryDocument) {
            for (unsigned i = 0; i < numDocuments; ++i) {
                docsNotified.insert(string(documents[i].ID));
                cerr << " " << documents[i].ID;
            }
        }
        cerr << "\n";
    }

    alloc_slice getServerCert() {
        FILE* f = fopen("vendor/couchbase-lite-core/Replicator/tests/data/cert.pem", "r");
        REQUIRE(f);
        char buf[10000];
        auto n = fread(buf, 1, sizeof(buf), f);
        REQUIRE(n > 0);
        REQUIRE(n < sizeof(buf));
        return alloc_slice(buf, n);
    }

    ~ReplicatorTest() {
        CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorStopped);
        CBLReplicator_Release(repl);
        CBLAuth_Free(config.authenticator);
        CBLEndpoint_Free(config.endpoint);
    }

    static vector<string> asVector(const set<string> strings) {
        vector<string> out;
        for (const string &s : strings)
            out.push_back(s);
        return out;
    }

};
