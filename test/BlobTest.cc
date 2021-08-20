//
// BlobTest.cc
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

#include "CBLTest.hh"
#include "CBLPrivate.h"
#include <string>

using namespace fleece;
using namespace std;


class BlobTest : public CBLTest { };


TEST_CASE_METHOD(BlobTest, "Check blob are equals", "[Blob]") {
    alloc_slice content1("This is the content of the blob 1.");
    alloc_slice content2("This is the content of the blob 2.");
    
    CBLBlob* blob1 = CBLBlob_CreateWithData("text/plain"_sl, content1);
    
    CBLBlob* blob2 = CBLBlob_CreateWithData("text/plain"_sl, content1);
    
    CBLError error;
    CBLBlobWriteStream* ws = CBLBlobWriter_Create(db, &error);
    CBLBlobWriter_Write(ws, content1.buf, content1.size, &error);
    CBLBlob* blob3 = CBLBlob_CreateWithStream("text/plain"_sl, ws);
    
    CBLBlob* blob4 = CBLBlob_CreateWithData("text/plain"_sl, content2);
    REQUIRE(blob4);
    
    CHECK(CBLBlob_Digest(blob1) == CBLBlob_Digest(blob2));
    CHECK(CBLBlob_Equals(blob1, blob2));
    CHECK(CBLBlob_Equals(blob1, blob3));
    
    CHECK(CBLBlob_Digest(blob1) != CBLBlob_Digest(blob4));
    CHECK(!CBLBlob_Equals(blob1, blob4));
    
    CBLBlob_Release(blob1);
    CBLBlob_Release(blob2);
    CBLBlob_Release(blob3);
    CBLBlob_Release(blob4);
}
