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


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blob") {
    const slice contents("This is the content of the blob.");
    const char *contentType = "text/plain";
    const string digest("sha1-gtf8MtnkloBRj0Od1CHA9LG69FM=");
    {
        MutableDocument doc("blobbo");

        unique_ptr<MutableBlob> blob;
        SECTION("Create with data") {
            blob.reset(new MutableBlob(db, contentType, contents));
        }
        SECTION("Create with stream") {
            BlobWriteStream writer(db);
            writer.write("This is the content "_sl);
            writer.write("of the blob."_sl);
            blob.reset(new MutableBlob(db, contentType, writer));
        }
        CHECK(blob->digest() == digest);
        CHECK(string(blob->contentType()) == contentType);
        CHECK(blob->length() == contents.size);
        MutableDict props = blob->properties();
        CHECK(props[kCBLTypeProperty].asString() == kCBLBlobType);
        CHECK(props[kCBLBlobDigestProperty].asString().asString() == digest);
        CHECK(props[kCBLBlobLengthProperty].asInt() == contents.size);
        CHECK(props[kCBLBlobContentTypeProperty].asString().asString() == contentType);
        doc["picture"] = props;
        db.saveDocument(doc);
    }

    Document doc = db.getDocument("blobbo");
    CHECK(doc.properties().toJSON(true,true).asString() == "{picture:{\"@type\":\"blob\","
          "content_type:\"text/plain\",digest:\"sha1-gtf8MtnkloBRj0Od1CHA9LG69FM=\",length:32}}");
    CHECK(Blob::isBlob(doc["picture"].asDict()));
    Blob blob(db, doc["picture"].asDict());
    CHECK(string(blob.contentType()) == contentType);
    CHECK(blob.length() == contents.size);

    {
        Blob::Contents c = blob.getContents();
        CHECK(slice(c.data, c.length) == contents);
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
}
