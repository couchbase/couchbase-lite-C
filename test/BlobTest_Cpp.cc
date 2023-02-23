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

#include "CBLTest_Cpp.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"
#include <string>

#include "cbl++/CouchbaseLite.hh"

using namespace std;
using namespace fleece;
using namespace cbl;

static constexpr size_t kDocIDBufferSize = 20;

static const slice  kBlobContents = "This is the content of the blob.";
static const string kBlobContentType = "text/plain";
static const string kBlobDigest = "sha1-gtf8MtnkloBRj0Od1CHA9LG69FM=";


static void checkBlob(Dict props) {
    CHECK(props[kCBLTypeProperty].asString() == kCBLBlobType);
    CHECK(props[kCBLBlobDigestProperty].asString() == kBlobDigest);
    CHECK(props[kCBLBlobLengthProperty].asInt() == kBlobContents.size);
    CHECK(props[kCBLBlobContentTypeProperty].asString() == kBlobContentType);
    CHECK(FLDict_IsBlob(props));
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
        CHECK(blob.contentType() == kBlobContentType);
        CHECK(blob.length() == kBlobContents.size);
        Dict props = blob.properties();
        checkBlob(props);

        CHECK(FLDict_GetBlob(props) == blob.ref());
        Blob gotBlob(props);
        CHECK(gotBlob == blob);

        // Add blob to document:
        doc["picture"] = props;

        CHECK(FLDict_IsBlob(props));
        Blob cachedBlob(props);
        CHECK(cachedBlob == blob);

        defaultCollection.saveDocument(doc);
    }
    {
        Document doc = defaultCollection.getDocument("blobbo");
        CHECK(doc.properties().toJSON(true,true).asString() == "{picture:{\"@type\":\"blob\","
              "content_type:\"text/plain\",digest:\"sha1-gtf8MtnkloBRj0Od1CHA9LG69FM=\",length:32}}");
        CHECK(Blob::isBlob(doc["picture"].asDict()));
        Blob blob(doc["picture"].asDict());
        REQUIRE(blob);
        CHECK(blob.contentType() == kBlobContentType);
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
    {
        // Compact the db and make sure the blob still exists: (issue #73)
        db.performMaintenance(kCBLMaintenanceTypeCompact);

        Document doc = defaultCollection.getDocument("blobbo");
        CHECK(Blob::isBlob(doc["picture"].asDict()));
        Blob blob(doc["picture"].asDict());
        REQUIRE(blob);
        CHECK(blob.loadContent() == kBlobContents);
    }
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blob in mutable doc", "[Blob]") {
    {
        MutableDocument doc("blobbo");
        Blob blob;
        blob = Blob(kBlobContentType, kBlobContents);
        Dict props = blob.properties();
        doc["picture"] = props;
        defaultCollection.saveDocument(doc);
    }

    Document doc = defaultCollection.getDocument("blobbo");
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
        Blob blob1(kBlobContentType, kBlobContents);
        array[0] = blob1;

        MutableDict dict = MutableDict::newDict();
        dict["b"] = Blob(kBlobContentType, kBlobContents);

        doc["array"] = array;
        doc["dict"] = dict;
        defaultCollection.saveDocument(doc);
    }

    Document doc = defaultCollection.getDocument("blobbo");
    auto array = doc["array"].asArray();
    auto dict =  doc["dict"].asDict();

    checkBlob(FLValue_AsDict(FLArray_Get(array, 0)));
    checkBlob(FLValue_AsDict(FLDict_Get(dict, "b"_sl)));
}


TEST_CASE_METHOD(CBLTest_Cpp, "C++ Blobs in ResultSet", "[Blob]") {
    for (int i = 0; i < 10; ++i) {
        char docID[kDocIDBufferSize];
        snprintf(docID, kDocIDBufferSize, "doc-%d", i);
        MutableDocument doc(docID);
        Blob blob(kBlobContentType, kBlobContents);
        doc["picture"] = blob.properties();
        defaultCollection.saveDocument(doc);
    }

    Query q(db, kCBLN1QLLanguage, "SELECT picture FROM _default");
    int rowCount = 0;
    for (Result row : q.execute()) {
        Dict picture = row[0].asDict();
        checkBlob(picture);
        Blob blob(picture);
        CHECK(blob.contentType() == kBlobContentType);
        CHECK(blob.loadContent() == kBlobContents);
        CHECK(blob.length() == kBlobContents.size);
        ++rowCount;
    }
    CHECK(rowCount == 10);
}
