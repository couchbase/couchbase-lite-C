# Couchbase Lite For C

This is a cross-platform version of the [Couchbase Lite][CBL] embedded NoSQL syncable database, with a plain C API. The API can be used directly, or as the substrate for binding to other languages like Python, JavaScript or Rust.

## Goals

- [x] C API
  - [x] Similar to to other Couchbase Lite platforms (Java, C#, Swift, Objective-C)
  - [x] Clean and regular design
  - [x] Comes with a C++ wrapper API, implemented as inline calls to C
  - [x] Can be bound to [other languages](#other-language-bindings) like Python, Go, or JavaScript
- [x] Same feature set as other Couchbase Lite platforms
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
- [x] Minimal platform dependencies: C++ standard library, filesystem, TCP/IP, libz (macOS / Linux), libicu (Linux)
- [x] Broad official OS support
  - [x] macOS (x86_64 and arm64)
  - [x] Debian 9/10 (including Raspbian / Raspberry Pi OS) and Ubuntu 20.04 (x86_64, armhf, arm64)
  - [x] Windows (x86_64 only)
  - [x] iOS (device and simulator archs)
  - [x] Android (armeabi-v7a, arm64-v8a, x86, x86_64)
- [x] Runs on Raspberry-Pi-level embedded platforms with…
  - 32-bit or 64-bit CPU
  - ARM or x86
  - Hundreds of MB RAM, hundreds of MHz CPU, tens of MB storage
  - Linux-based OS

## Examples

### C

```c
// Open a database:
CBLError error;
CBLDatabaseConfiguration config = {FLSTR("/tmp")};
CBLDatabase* db = CBLDatabase_Open(FLSTR("my_db"), &config, &error);

// Create a document:
CBLDocument* doc = CBLDocument_CreateWithID(FLSTR("foo"));
FLMutableDict props = CBLDocument_MutableProperties(doc);
FLSlot_SetString(FLMutableDict_Set(props, FLStr("greeting")), FLStr("Howdy!"));

// Save the document:
CBLDatabase_SaveDocument(db, doc, &error);
CBLDocument_Release(doc);

// Read it back:
const CBLDocument *readDoc = CBLDatabase_GetDocument(db, FLSTR("foo"), &error);
FLDict readProps = CBLDocument_Properties(readDoc);
FLSlice greeting = FLValue_AsString( FLDict_Get(readProps, FLStr("greeting")) );
CBLDocument_Release(readDoc);
```

### Others

**NOTE**: The C++ API is not part of the official release.

```cpp
// Open a database:
cbl::Database db(kDatabaseName, {"/tmp"});

// Create a document:
cbl::MutableDocument doc("foo");
doc["greeting"] = "Howdy!";
db.saveDocument(doc);

// Read it back:
cbl::Document readDoc = db.getMutableDocument("foo");
fleece::Dict readProps = readDoc.properties();
fleece::slice greeting = readProps["greeting"].asString();
```

## Documentation

* [Couchbase Lite documentation](https://docs.couchbase.com/couchbase-lite/current/introduction.html) is a must-read to learn the architecture and the API concepts, even though the API details are different here.
* API documentation in [the header files](https://github.com/couchbaselabs/couchbase-lite-C/tree/master/include/cbl)
* [Using Fleece](https://github.com/couchbaselabs/fleece/wiki/Using-Fleece) — Fleece is the API for document properties
* [Contributor guidelines](CONTRIBUTING.md), should you wish to submit bug fixes, tests, or other improvements

## Building It

### Source Code

This project contains submodules in multiple levels. Please make sure all submodules are checked out before building the project.

```
git clone https://github.com/couchbase/couchbase-lite-C.git
cd couchbase-lite-C
git submodule update --init --recursive
```

### Unix (Linux / macOS)

**Dependencies:**

* GCC 7+ or Clang
* CMake 3.10+
* (Linux) ICU development libraries (`apt-get install icu-dev`)
* (Linux) ZLIB development libraries (`apt-get install zlib1g-dev`)

**Steps:**

1. Prepare and build project with CMake. 

	```
	# At the project directory, create a build directory:
	mkdir build && cd build
	
	# Prepare project. Specify CMAKE_INSTALL_PREFIX for the installation directory when running `make install`.
	# Add -DSTRIP_SYMBOLS=ON to generate a separate debug symbol file and strip private symbols from the built shared library.
	cmake -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=`pwd`/output ..
	
	# Build:
	make
	```
	
2. Install the built library and headers to the location specified by `CMAKE_INSTALL_PREFIX`

	```
	make install
	```

	The installation will contain header files in the `include` folder and a shared library file in the `lib` folder.

### Linux (Cross Compile)

**NOTE**: Due to the complexity of cross compiling, these instructions are very high level on purpose.  If you are curious about more fine-grained details you can check out the [cross compilation script](https://raw.githubusercontent.com/couchbase/couchbase-lite-C/master/jenkins/ci_cross_build.py) we use.

**Dependencies:**

* Cross compiler based on GCC 7+ or clang
* CMake 3.10+
* (in sysroot) ICU development libraries
* (in sysroot) ZLIB development libraries

**Steps:**

The steps are similar to the steps for building the regular linux library above. The difference is that you will also need
to prepare the CMake toolchain file that sets up the cross compiler and `sysroot` for the cross compilation. 

When running the `cmake` to prepare the build project, add `CMAKE_TOOLCHAIN_FILE` option to specify the CMake toolchain file:

```
cmake -DCMAKE_TOOLCHAIN_FILE=<YOUR/toolchain.cmake> -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=`pwd`/output ..
```

### Windows

**Dependencies:**

* Visual Studio Toolset v141+ (Visual Studio 2017+)
* CMake 3.10+

**Steps:**

1. Prepare project with CMake.

	```
	# Using powershell, at the project directory, create a build directory:
	md build
	cd build

	# Prepare project. Specify CMAKE_INSTALL_PREFIX for the installation directory:
	cmake -G "Visual Studio 15 2017" -A x64 -DCMAKE_INSTALL_PREFIX="$pwd/output" ..

	```
2. Build and Install

	```
	cmake --build . --config MinSizeRel --target install
	```
	
	The installation will contain header files in the `include` folder, a dll file in the `bin` folder and a lib file in the `lib` folder.

### Xcode on macOS

:warning: Do not use Xcode 13 because of a [downstream issue](https://github.com/ARMmbed/mbedtls/issues/5052) :warning:

1. Open the Xcode project `CBL_C.xcodeproj`
2. Select scheme `CBL_C Framework`
3. Build

The result is `CouchbaseLite.framework` in your `Xcode Build Products/Debug` directory (the path depends on your Xcode settings.)

## Testing It

### CMake on Unix

1. Follow the steps in building
2. Build the test project `make CBL_C_Tests`
3. Run the tests in `test` directory `./CBL_C_Tests -r list` (for cross compile, this step needs to be done on the target system)

### CMake on Windows

1. Follow the steps in building
2. Build the test project `cmake --build . --target CBL_C_Tests`
3. Run the tests in `test\Debug` directory `.\CBL_C_Tests.exe -r list`

### With Xcode on macOS

1. Open the Xcode project from the Building section
2. Select scheme `CBL_Tests`
3. Run

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

All of these (even C++) have no official support by Couchbase.

If you've created a language binding, please let us know by filing an issue, or a PR that updates the list below.

* **C++**: Already included; see [`include/cbl++`](https://github.com/couchbase/couchbase-lite-C/tree/master/include/cbl%2B%2B)
* **Go** (Golang): [Third-party, in progress](https://github.com/svr4/couchbase-lite-cgo)
* **Nim**: [couchbase-lite-nim](https://github.com/couchbaselabs/couchbase-lite-nim)
* **Python**: [couchbase-lite-python](https://github.com/couchbaselabs/couchbase-lite-python)
* **Rust**: [couchbase-lite-rust](https://github.com/couchbaselabs/couchbase-lite-rust)
