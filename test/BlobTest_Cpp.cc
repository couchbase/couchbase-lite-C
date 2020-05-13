//
// BlobTest_Cpp.cc
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
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <string>

#include "cbl++/CouchbaseLite.hh"

using namespace std;
using namespace fleece;
using namespace cbl;


static const slice kBlobContents("This is the content of the blob.");
static const char *kBlobContentType = "text/plain";
static const string kBlobDigest("sha1-gtf8MtnkloBRj0Od1CHA9LG69FM=");


static void checkBlob(Dict props) {
    CHECK(props[kCBLTypeProperty].asString() == kCBLBlobType);
    CHECK(props[kCBLBlobDigestProperty].asString().asString() == kBlobDigest);
    CHECK(props[kCBLBlobLengthProperty].asInt() == kBlobContents.size);
    CHECK(props[kCBLBlobContentTypeProperty].asString().asString() == kBlobContentType);
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blob", "[Blob]") {
    {
        MutableDocument doc("blobbo");

        Blob blob;
        SECTION("Create with data") {
            blob = Blob(kBlobContentType, kBlobContents);
        }
        SECTION("Create with stream") {
            BlobWriteStream writer(db);
            writer.write("This is the content "_sl);
            writer.write("of the blob."_sl);
            blob = Blob(kBlobContentType, writer);
        }
        CHECK(blob.digest() == kBlobDigest);
        CHECK(string(blob.contentType()) == kBlobContentType);
        CHECK(blob.length() == kBlobContents.size);
        Dict props = blob.properties();
        checkBlob(props);

        CHECK(CBLBlob_Get(props) == nullptr);

        // Add blob to document:
        doc["picture"] = props;
        db.saveDocument(doc);
    }

    Document doc = db.getDocument("blobbo");
    CHECK(doc.properties().toJSON(true,true).asString() == "{picture:{\"@type\":\"blob\","
          "content_type:\"text/plain\",digest:\"sha1-gtf8MtnkloBRj0Od1CHA9LG69FM=\",length:32}}");
    CHECK(Blob::isBlob(doc["picture"].asDict()));
    Blob blob(doc["picture"].asDict());
    REQUIRE(blob);
    CHECK(string(blob.contentType()) == kBlobContentType);
    CHECK(blob.length() == kBlobContents.size);

    {
        CHECK(blob.loadContent() == kBlobContents);
    }

    char buf[10];
    {
        unique_ptr<BlobReadStream> in(blob.openContentStream());
        size_t n = in->read(buf, 10);
        CHECK(string(buf, n) == "This is th");
        n = in->read(buf, 10);
        CHECK(string(buf, n) == "e content ");
        n = in->read(buf, 10);
        CHECK(string(buf, n) == "of the blo");
        n = in->read(buf, 10);
        CHECK(string(buf, n) == "b.");
        n = in->read(buf, 10);
        CHECK(n == 0);
    }

    {
        unique_ptr<BlobReadStream> in(blob.openContentStream());
        size_t n = in->read(buf, 10);
        CHECK(string(buf, n) == "This is th");
    }

    Blob blob2(doc["picture"].asDict());
    CHECK(blob2 == blob);
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blob in mutable doc", "[Blob]") {
    {
        MutableDocument doc("blobbo");
        Blob blob;
        blob = Blob(kBlobContentType, kBlobContents);
        Dict props = blob.properties();
        doc["picture"] = props;
        db.saveDocument(doc);
    }

    Document doc = db.getDocument("blobbo");
    Dict props = doc["picture"].asDict();
    checkBlob(props);

    Blob blob = Blob(props);
    REQUIRE(blob);
    CHECK((FLDict)blob.properties() == (FLDict)props);
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blobs in arrays/dicts", "[Blob]") {
    {
        MutableDocument doc("blobbo");
        MutableArray array = MutableArray::newArray();
        array.insertNulls(0, 1);
        CBLBlob *blob1 = CBLBlob_CreateWithData(kBlobContentType, kBlobContents);
        FLSlot_SetBlob(array[0], blob1);

        MutableDict dict = MutableDict::newDict();
        CBLBlob *blob2 = CBLBlob_CreateWithData(kBlobContentType, kBlobContents);
        FLSlot_SetBlob(dict["b"], blob2);

        doc["array"] = array;
        doc["dict"] = dict;
        db.saveDocument(doc);

        CBLBlob_Release(blob1);
        CBLBlob_Release(blob2);
    }

    Document doc = db.getDocument("blobbo");
    auto array = doc["array"].asArray();
    auto dict =  doc["dict"].asDict();

    checkBlob(FLValue_AsDict(FLArray_Get(array, 0)));
    checkBlob(FLValue_AsDict(FLDict_Get(dict, "b"_sl)));
}
