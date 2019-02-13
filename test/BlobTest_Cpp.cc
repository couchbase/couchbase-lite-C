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


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blob") {
    {
        MutableDocument doc("blobbo");

        MutableBlob blob;
        SECTION("Create with data") {
            blob = MutableBlob(kBlobContentType, kBlobContents);
        }
        SECTION("Create with stream") {
            BlobWriteStream writer(db);
            writer.write("This is the content "_sl);
            writer.write("of the blob."_sl);
            blob = MutableBlob(kBlobContentType, writer);
        }
        CHECK(blob.digest() == kBlobDigest);
        CHECK(string(blob.contentType()) == kBlobContentType);
        CHECK(blob.length() == kBlobContents.size);
        MutableDict props = blob.properties();
        checkBlob(props);

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
        Blob::Contents c = blob.getContents();
        CHECK(slice(c.data, c.length) == kBlobContents);
    }

    char buf[10];
    blob.openContentStream();
    size_t n = blob.readContent(buf, 10);
    CHECK(string(buf, n) == "This is th");
    n = blob.readContent(buf, 10);
    CHECK(string(buf, n) == "e content ");
    n = blob.readContent(buf, 10);
    CHECK(string(buf, n) == "of the blo");
    n = blob.readContent(buf, 10);
    CHECK(string(buf, n) == "b.");
    n = blob.readContent(buf, 10);
    CHECK(n == 0);
    blob.closeContentStream();

    blob.openContentStream();
    n = blob.readContent(buf, 10);
    CHECK(string(buf, n) == "This is th");

    Blob blob2(doc["picture"].asDict());
    CHECK(blob2 == blob);
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blob in mutable doc") {
    MutableDocument doc("blobbo");
    MutableBlob blob;
    blob = MutableBlob(kBlobContentType, kBlobContents);
    MutableDict props = blob.properties();
    doc["picture"] = props;
    db.saveDocument(doc);

    doc = db.getMutableDocument("blobbo");
    props = doc.properties().getMutableDict("picture"_sl);
    checkBlob(props);

    blob = MutableBlob(props);
    REQUIRE(blob);
    CHECK((FLDict)blob.properties() == (FLDict)props);
}
