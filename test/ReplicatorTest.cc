//
// ReplicatorTest.cc
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

#include "CBLTest.hh"
#include "cbl++/CouchbaseLite.hh"
#include <iostream>
#include <thread>

using namespace std;
using namespace fleece;
using namespace cbl;


TEST_CASE_METHOD(CBLTest_Cpp, "Fake Replicate") {
    ReplicatorConfiguration config(db);
    config.replicatorType = kCBLReplicatorTypePull;
#if 1
    config.endpoint.setURL("wss://couchbase.com/bogus");
#else
    config.endpoint.setURL("ws://localhost:4984/itunes");
#endif

    Replicator repl(config);
    auto ctoken = repl.addChangeListener([&](Replicator r, const CBLReplicatorStatus &status) {
        CHECK(r == repl);
        cerr << "--- PROGRESS: status=" << status.activity << ", fraction=" << status.progress.fractionComplete << ", err=" << status.error.domain << "/" << status.error.code << "\n";
    });
    auto dtoken = repl.addDocumentListener([&](Replicator r, bool isPush,
                                               const vector<CBLReplicatedDocument> &docs) {
        CHECK(r == repl);
        cerr << "--- " << docs.size() << " docs " << (isPush ? "pushed" : "pulled") << ":";
        for (auto &doc : docs)
            cerr << " " << doc.ID;
        cerr << "\n";
    });
    repl.start();

    cerr << "Waiting...\n";
    CBLReplicatorStatus status;
    while ((status = repl.status()).activity != kCBLReplicatorStopped) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    cerr << "Finished with activity=" << status.activity
         << ", error=(" << status.error.domain << "/" << status.error.code << ")\n";
}
