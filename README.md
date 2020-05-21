# Couchbase Lite For C

This is a cross-platform version of the [Couchbase Lite][CBL] embedded NoSQL syncable database, with a plain C API. The API can be used directly, or as the substrate for binding to other languages like Python, JavaScript or Rust.

## What's New (May 2020)

**This project is close to beta status.** The API is nearly complete and almost all of the functionality is implemented, but there are still missing pieces and only limited testing.

* New Rust and Nim language bindings! They're in the new top-level `bindings` directory. The Python binding has been moved there too.
* Added variants of many API functions, which take slices (`FLSlice`) instead of C strings. These are more efficient to call from languages whose native string type is not NUL-terminated, such as Rust.
* Added `CBLResultSet_RowArray()`, `CBLResultSet_RowDict()`, and `CBLResultSet_GetQuery()`.
* More of the logging API (in `CBLLog.h`) is implemented, including custom log callbacks.
* Updated to latest Couchbase Lite Core (LiteCore).

## Goals

- [x] C API
  - [x] Similar to to other Couchbase Lite platforms (Java, C#, Swift, Objective-C)
  - [x] Clean and regular design
  - [x] Comes with a C++ wrapper API, implemented as inline calls to C
  - [x] Experimental Python binding (made using `cffi`)
  - [x] Can be bound to [other languages](#other-language-bindings) like Go or JavaScript
- [ ] Same feature set as other Couchbase Lite platforms
  - [x] Schemaless JSON data model
      - [x] Standard CRUD operations
      - [x] Efficient binary blob support
      - [x] Timed document expiration
      - [x] Database encryption (Enterprise Edition only)
  - [x] Powerful query language based on Couchbase's [N1QL][N1QL]
      - [x] Index arbitrary JSON properties or derived expression values
      - [x] Full-text search (FTS)
  - [x] Multi-master bidirectional replication (sync) with Couchbase Server
      - [x] Fast WebSocket-based protocol
      - [x] Transfers document deltas for minimal bandwidth
      - [x] Replicator event listeners
      - [x] Replicator online/offline support and retry behavior
      - [x] Replicator TLS/SSL support
      - [ ] Peer-to-peer replication
- [x] Minimal platform dependencies: C++ standard library, filesystem, TCP/IP
- [ ] Broad OS support
  - [x] macOS, for ease of development
  - [x] Common Linux distros, esp. Ubuntu, Fedora, Raspbian (q.v.)
  - [x] Windows
  - [x] iOS
  - [ ] Android (but we have a [Couchbase Lite For Android][ANDROID] already, with a Java API)
- [x] Runs on Raspberry-Pi-level embedded platforms with…
  - 32-bit or 64-bit CPU
  - ARM or x86
  - Hundreds of MB RAM, hundreds of MHz CPU, tens of MB storage
  - Linux-based OS
  - Stretch goal: Simpler embedded kernels like mbedOS or ESP-IDF.

## Examples

### C

```c
// Open a database:
CBLError error;
CBLDatabaseConfiguration config = {"/tmp", kCBLDatabase_Create};
CBLDatabase* db = CBLDatabase_Open("my_db", &config, &error);

// Create a document:
CBLDocument* doc = CBLDocument_New("foo");
FLMutableDict props = CBLDocument_MutableProperties(doc);
FLSlot_SetString(FLMutableDict_Set(dict, FLStr("greeting")), FLStr("Howdy!"));

// Save the document:
const CBLDocument *saved = CBLDatabase_SaveDocument(db, doc,
                                         kCBLConcurrencyControlFailOnConflict,
                                         &error);
CBLDocument_Release(saved);
CBLDocument_Release(doc);

// Read it back:
const CBLDocument *readDoc = CBLDatabase_GetDocument(db, "foo");
FLDict readProps = CBLDocument_Properties(readDoc);
FLSlice greeting = FLValue_AsString( FLDict_Get(readProps, FLStr("greeting")) );
CBLDocument_Release(readDoc);
```

### C++

```cpp
// Open a database:
cbl::Database db(kDatabaseName, {"/tmp", kCBLDatabase_Create});

// Create a document:
cbl::MutableDocument doc("foo");
doc["greeting"] = "Howdy!";
db.saveDocument(doc);

// Read it back:
cbl::Document doc = db.getMutableDocument("foo");
fleece::Dict readProps = doc->properties();
fleece::slice greeting = readProps["greeting"].asString();
```

### Nim

```nim
# Open a database:
let config = DatabaseConfiguration(directory: "/tmp", flags: {DatabaseFlag.create})
var db = openDatabase("nim_db", config)

# Create a document:
var doc = newDocument("foo")
doc["greeting"] = "Howdy!"
db.saveDocument(doc)

# Read it back:
let readDoc = db.getDocument("foo")
let readProps = readDoc.properties
let greeting = readProps["greeting"]
```

### Python

```python
# Open a database:
db = Database("python_db", DatabaseConfiguration("/tmp"));

# Create a document:
doc = MutableDocument("foo")
doc["greeting"] = "Howdy!"
db.saveDocument(doc)

# Read it back:
readDoc = db.getDocument("foo")
readProps = readDoc.properties
greeting = readProps["greeting"]
```

### Rust

```rust
// Open a database:
let cfg = DatabaseConfiguration{directory: tmp_dir.path(), flags: CREATE};
let mut db = Database::open("rust_db, Some(cfg)).expect("opening db");

// Create a document:
let mut doc = Document::new_with_id("foo");
let mut props = doc.mutable_properties();
props.at("greeting").put_string("Howdy!");
db.save_document(&mut doc, ConcurrencyControl::FailOnConflict).expect("saving");

// Read it back:
let doc = db.get_document("foo").expect("reload document");
let props = doc.properties();
let greeting = props.get("greeting");
```

## Documentation

* [Couchbase Lite documentation](https://docs.couchbase.com/couchbase-lite/2.7/introduction.html) is a must-read to learn the architecture and the API concepts, even though the API details are different here.
* [C API documentation](http://labs.couchbase.com/couchbase-lite-C/C/html/modules.html) (generated by Doxygen)
* Or you could just [read the header files](https://github.com/couchbaselabs/couchbase-lite-C/tree/master/include/cbl)
* [Using Fleece](https://github.com/couchbaselabs/fleece/wiki/Using-Fleece) — Fleece is the API for document properties
* [Contributor guidelines](CONTRIBUTING.md), should you wish to submit bug fixes, tests, or other improvements

## Building It

### With CMake on Unix (now including Raspberry Pi!)

Dependencies:
* GCC 7+ or Clang
* CMake 3.9+
* ICU libraries (`apt-get install icu-dev`)

1. Clone the repo
2. Check out submodules (recursively), i.e. `git submodule update --init --recursive`
3. Run the shellscript `build.sh`
6. Run the unit tests, with `test.sh`

The library is at `build_cmake/libCouchbaseLiteC.so`. (Or `.DLL` or `.dylib`)

### With CMake on Windows

_(Much like building on Unix. Details TBD)_

### With Xcode on macOS

1. Clone the repo
2. Check out submodules (recursively)
3. Open the Xcode project in the `Xcode` subfolder
4. Select scheme `CBL_C Framework`
5. Build

The result is `CouchbaseLite.framework` in your `Xcode Build Products/Debug` directory (the path depends on your Xcode settings.)

To run the unit tests:

4. Select scheme `CBL_Tests`
5. Run

## Using It

### Generic instructions

* In your build settings, add the paths `include` and `vendor/couchbase-lite-core/vendor/fleece/API` (relative to this repo) to your header search paths.
* In your linker settings, add the dynamic library.
* In source files, add `#include "cbl/CouchbaseLite.h"`.

### With Xcode on macOS

* Add `CouchbaseLite.framework` (see instructions above) to your project.
* In source files, add `#include "CouchbaseLite/CouchbaseLite.h"`.
* You may need to add a build step to your target, to copy the framework into your app bundle.


[CBL]:        https://www.couchbase.com/products/lite
[N1QL]:       https://www.couchbase.com/products/n1ql
[BUILD_LINUX]:https://github.com/couchbase/couchbase-lite-core/wiki/Build-and-Deploy-On-Linux#dependencies
[IOS]:        https://github.com/couchbase/couchbase-lite-ios
[ANDROID]:    https://github.com/couchbase/couchbase-lite-android

## Other Language Bindings

* **C++**: Already included; see [`include/cbl++`](https://github.com/couchbaselabs/couchbase-lite-C/tree/master/include/cbl%2B%2B)
* **Python**: [Included](bindings/python/README.md) but unsupported
* **Nim**: [Included](bindings/nim/README.md) but unsupported
* **Rust**: [Included](bindings/rust/README.md) but unsupported
* **Go** (Golang): [Third-party, in progress](https://github.com/svr4/couchbase-lite-cgo).
