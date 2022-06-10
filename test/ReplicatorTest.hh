//
// ReplicatorTest.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "CBLTest_Cpp.hh"
#include <chrono>
#include <iostream>
#include <thread>
#include <set>
#include <vector>

using namespace std;
using namespace fleece;
using namespace cbl;

CBL_START_WARNINGS_SUPPRESSION
CBL_IGNORE_DEPRECATED_API

class ReplicatorTest : public CBLTest_Cpp {
public:
    using clock    = std::chrono::high_resolution_clock;
    using time     = clock::time_point;
    using seconds  = std::chrono::duration<double, std::ratio<1,1>>;
    
    enum class IdleAction {
        kStopReplicator,    ///< Stop Replicator
        kContinueMonitor,   ///< Continue checking status
        kFinishMonitor      ///< Finish checking status
    };

    CBLReplicatorConfiguration config = {};
    CBLReplicator *repl = nullptr;
    
    bool enableDocReplicationListener = true;
    bool logEveryDocument = true;
    set<string> replicatedDocIDs;
    vector<CBLReplicatedDocument> replicatedDocs;
    
    CBLError replError = {};
    IdleAction idleAction = IdleAction::kStopReplicator;
    double timeoutSeconds = 30.0;

    ReplicatorTest() {
        config.database = db.ref();
        config.replicatorType = kCBLReplicatorTypePull;
        config.context = this;
    }

    void replicate(CBLError expectedError = {}) {
        CBLError error;
        CBLReplicatorStatus status;
        if (!repl) {
            repl = CBLReplicator_Create(&config, &error);
            status = CBLReplicator_Status(repl);
            CHECK(status.activity == kCBLReplicatorStopped);
            CHECK(status.progress.complete == 0.0);
            CHECK(status.progress.documentCount == 0);
            CHECK(status.error.code == 0);
        }
        REQUIRE(repl);

        auto ctoken = CBLReplicator_AddChangeListener(repl, [](void *context, CBLReplicator *r,
                                                               const CBLReplicatorStatus *status) {
            ((ReplicatorTest*)context)->statusChanged(r, *status);
        }, this);

        
        CBLListenerToken* dtoken = nullptr;
        if (enableDocReplicationListener) {
            dtoken = CBLReplicator_AddDocumentReplicationListener(repl, [](void *context, CBLReplicator *r,
                                                                           bool isPush,
                                                                           unsigned numDocuments,
                                                                           const CBLReplicatedDocument* documents) {
                ((ReplicatorTest*)context)->docProgress(r, isPush, numDocuments, documents);
            }, this);
        }
        
        CBLReplicator_Start(repl, false);

        time start = clock::now();
        cerr << "Waiting...\n";
        while (std::chrono::duration_cast<seconds>(clock::now() - start).count() < timeoutSeconds) {
            status = CBLReplicator_Status(repl);
            if (config.continuous && status.activity == kCBLReplicatorIdle) {
                if (idleAction == IdleAction::kStopReplicator) {
                    cerr << "Stop the continuous replicator...\n";
                    CBLReplicator_Stop(repl);
                } else if (idleAction == IdleAction::kFinishMonitor) {
                    break;
                }
            } else if (status.activity == kCBLReplicatorStopped)
                break;
            this_thread::sleep_for(100ms);
        }
        cerr << "Finished with activity=" << static_cast<int>(status.activity)
             << ", complete=" << status.progress.complete
             << ", documentCount=" << status.progress.documentCount
             << ", error=(" << status.error.domain << "/" << status.error.code << ")\n";
        
        if (config.continuous && idleAction == IdleAction::kFinishMonitor)
            CHECK(status.activity == kCBLReplicatorIdle);
        else
            CHECK(status.activity == kCBLReplicatorStopped);
        
        if (expectedError.code > 0) {
            CHECK(status.error.code == expectedError.code);
            CHECK(status.progress.complete < 1.0);
        } else {
            CHECK(status.error.code == 0);
            CHECK(status.progress.complete == 1.0);
        }
        
        CBLListener_Remove(ctoken);
        CBLListener_Remove(dtoken);
    }
    
    void resetReplicator() {
        if (!repl)
            return;
        
        REQUIRE(CBLReplicator_Status(repl).activity == kCBLReplicatorStopped);
        CBLReplicator_Release(repl);
        repl = nullptr;
    }
    
    bool waitForActivityLevel(CBLReplicatorActivityLevel level, double timeout) {
        time start = clock::now();
        while (std::chrono::duration_cast<seconds>(clock::now() - start).count() < timeout) {
            if (CBLReplicator_Status(repl).activity == level)
                return true;
            this_thread::sleep_for(100ms);
        }
        return false;
    }

    void statusChanged(CBLReplicator *r, const CBLReplicatorStatus &status) {
        CHECK(r == repl);
        cerr << "--- PROGRESS: status=" << static_cast<int>(status.activity)
             << ", fraction=" << status.progress.complete
             << ", err=" << status.error.domain << "/" << status.error.code << "\n";
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
                replicatedDocIDs.insert(string(documents[i].ID));
                replicatedDocs.push_back(documents[i]);
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
        if (repl) {
            CHECK(CBLReplicator_Status(repl).activity == kCBLReplicatorStopped);
            CBLReplicator_Release(repl);
        }
        CBLAuth_Free(config.authenticator);
        CBLEndpoint_Free(config.endpoint);
        
        // For async clean up in Replicator:
        this_thread::sleep_for(200ms);
    }

    static vector<string> asVector(const set<string> strings) {
        vector<string> out;
        for (const string &s : strings)
            out.push_back(s);
        return out;
    }

};

CBL_STOP_WARNINGS_SUPPRESSION
