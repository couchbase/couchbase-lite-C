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


TEST_CASE_METHOD(BlobTest, "Create blob stream and close", "[Blob]") {
    alloc_slice content("This is the content of the blob 1.");
    CBLError error;
    CBLBlobWriteStream* ws = CBLBlobWriter_Create(db, &error);
    CBLBlobWriter_Write(ws, content.buf, content.size, &error);
    CBLBlobWriter_Close(ws);
}


TEST_CASE_METHOD(BlobTest, "Create blob with stream", "[Blob]") {
    static constexpr slice kBlobContent = "This is the content of the blob 1.";
    CBLError error;
    CBLBlob *blob;
    {
        CBLBlobWriteStream* ws = CBLBlobWriter_Create(db, &error);
        REQUIRE(ws);
        REQUIRE(CBLBlobWriter_Write(ws, &kBlobContent[0], 10, &error));
        REQUIRE(CBLBlobWriter_Write(ws, &kBlobContent[10], kBlobContent.size - 10, &error));
        blob = CBLBlob_CreateWithStream("text/plain"_sl, ws);
        REQUIRE(blob);
        // Note: After creating a blob with the stream, the created blob will take
        // ownership of the stream so do not close the stream.
    }

    // Set blob in a document and save:
    auto doc = CBLDocument_CreateWithID("doc1"_sl);
    auto props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetBlob(props, "blob"_sl, blob);
    CHECK(CBLDatabase_SaveDocument(db, doc, &error));
    
    // Read content as a slice:
    {
        FLSliceResult gotContent = CBLBlob_Content(blob, &error);
        CHECK(gotContent == kBlobContent);
        FLSliceResult_Release(gotContent);
    }

    // Read content as stream:
    {
        static_assert(kBlobContent.size == 34, "the checks below assume the blob is 34 bytes long");
        char buf[20];
        CBLBlobReadStream *in = CBLBlob_OpenContentStream(blob, &error);
        REQUIRE(in);
        CHECK(CBLBlobReader_Position(in) == 0);
        CHECK(CBLBlobReader_Read(in, buf, 20, &error) == 20);
        CHECK(memcmp(buf, &kBlobContent[0], 20) == 0);

        CHECK(CBLBlobReader_Position(in) == 20);
        CHECK(CBLBlobReader_Read(in, buf, 20, &error) == 14);
        CHECK(memcmp(buf, &kBlobContent[20], 14) == 0);

        CHECK(CBLBlobReader_Position(in) == 34);
        CHECK(CBLBlobReader_Read(in, buf, 20, &error) == 0);

        CHECK(CBLBlobReader_Seek(in, 12, kCBLSeekModeFromStart, &error) == 12);
        CHECK(CBLBlobReader_Position(in) == 12);
        CHECK(CBLBlobReader_Read(in, buf, 7, &error) == 7);
        CHECK(memcmp(buf, &kBlobContent[12], 7) == 0);
        CHECK(CBLBlobReader_Position(in) == 12 + 7);

        CHECK(CBLBlobReader_Seek(in, 1, kCBLSeekModeRelative, &error) == 20);
        CHECK(CBLBlobReader_Position(in) == 20);

        CHECK(CBLBlobReader_Seek(in, -5, kCBLSeekModeFromEnd, &error) == kBlobContent.size - 5);
        CHECK(CBLBlobReader_Position(in) == kBlobContent.size - 5);

        // seek past EOF is not error, but pos is pinned to the EOF
        CHECK(CBLBlobReader_Seek(in, 9999, kCBLSeekModeFromStart, &error) == kBlobContent.size);
        CHECK(CBLBlobReader_Position(in) == kBlobContent.size);

        // but seek to a negative position is an error
        ExpectingExceptions x;
        CHECK(CBLBlobReader_Seek(in, -999, kCBLSeekModeFromEnd, &error) < 0);
        CHECK(error.domain == kCBLDomain);
        CHECK(error.code == kCBLErrorInvalidParameter);
        CHECK(CBLBlobReader_Position(in) == kBlobContent.size);

        CBLBlobReader_Close(in);
    }

    CBLBlob_Release(blob);
    CBLDocument_Release(doc);
}


TEST_CASE_METHOD(BlobTest, "Create JSON from Blob", "[Blob]") {
    alloc_slice content1("This is the content of the blob 1.");
    CBLBlob* blob = CBLBlob_CreateWithData("text/plain"_sl, content1);
    CHECK(alloc_slice(CBLBlob_CreateJSON(blob)) == "{\"@type\":\"blob\",\"content_type\":\"text/plain\",\"digest\":\"sha1-dXNgUcxC3n7lxfrYkbLUG4gOKRw=\",\"length\":34}"_sl);
    
    CBLError error;
    auto doc = CBLDocument_CreateWithID("doc1"_sl);
    auto props = CBLDocument_MutableProperties(doc);
    FLMutableDict_SetBlob(props, "blob"_sl, blob);
    REQUIRE(CBLDatabase_SaveDocument(db, doc, &error));
    CBLBlob_Release(blob);
    CBLDocument_Release(doc);
    
    doc = CBLDatabase_GetMutableDocument(db, "doc1"_sl, &error);
    REQUIRE(doc);
    FLValue value = FLDict_Get(CBLDocument_Properties(doc), "blob"_sl);
    const CBLBlob* gotBlob = FLValue_GetBlob(value);
    CHECK(alloc_slice(CBLBlob_CreateJSON(gotBlob)) == "{\"content_type\":\"text/plain\",\"digest\":\"sha1-dXNgUcxC3n7lxfrYkbLUG4gOKRw=\",\"length\":34,\"@type\":\"blob\"}"_sl);
    CBLDocument_Release(doc);
}
