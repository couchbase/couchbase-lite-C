//
// PerfTest.cc
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
#include "Stopwatch.hh"
#include <fstream>

using namespace std;
using namespace fleece;


// NOTE: This file is large (~30MB) so it isn't checked into the repo.
//FIXME: Not a portable path.
static constexpr const char *kJSONFilePath = "../DataSets/travel-sample/travelSample.json";


TEST_CASE_METHOD(CBLTest_Cpp, "Benchmark Import JSON", "[.Perf]") {
    Stopwatch st;

    db.createIndex("types",      {kCBLValueIndex, "[[\".type\"]]"});
    db.createIndex("locations",  {kCBLValueIndex, "[[\".country\"], [\".city\"]]"});
    db.createIndex("longitudes", {kCBLValueIndex, "[[\".geo.lon\"]]"});

    ImportJSONLines(kJSONFilePath, db.ref());

    cout << "Elapsed time: " << st.elapsed() << " sec\n";
}
