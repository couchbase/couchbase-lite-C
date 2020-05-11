# Rust Language Bindings For Couchbase Lite

10 May 2020

The `CouchbaseLite` subdirectory of this directory is a Cargo package containing a [Couchbase
Lite][CBL] API binding for the [Rust language][RUST].

It's still in early development. It's incomplete, has only had some informal testing, and that
only on macOS. Also I (Jens) am a newbie at Rust and may not be doing things the write way.
Feedback gratefully accepted.

## Prerequisites

In addition to Rust, you'll need to install [bindgen][BINDGEN], which generates Rust FFI APIs
from C headers. Installation instructions are [here][BINDGEN_INSTALL] -- the main thing you'll need
to do is install Clang.

## Building

**_"Some assembly required..."_**

You first need to build Couchbase Lite For C (the root of this repo) with CMake, by running the
`build.sh` script in the repo root directory. That will produce the shared library.

    $ cd ../..
    $ ./build.sh

After that, go to the `CouchbaseLite` package directory and fix the hardcoded path I left in the
`build.rs` file: it's the value of `DEFAULT_LIBCLANG_PATH` at line 32. You'll need to find out where
your LLVM installation directory is, and set this string constant to the path to its `lib`
subdirectory.

    $ cd bindings/rust/CouchbaseLite/
    $ my_favorite_editor build.rs

Now that this is set up, you can build normally with Cargo:

    $ cargo build

**Unit tests must be run single-threaded.** This is because each test case checks for leaks by
counting the number of extant Couchbase Lite objects before and after it runs, and failing if the
number increases. This works only if a single test runs at a time.

    $ cargo test -- --test-threads 1

The library itself has no thread-safety problems; if you want to run the tests multi-threaded, just
edit `tests/simple_tests.rs` and change the value of `LEAK_CHECKS` to `false`.

## Learning

I've copied the doc-comments from the C API into the Rust files. But Couchbase Lite is fairly
complex, so if you're not already familiar with it, you'll want to start by reading through
the [official documentation][CBLDOCS].

The Rust API is mostly method-for-method compatible with the languages documented there, except
down at the document property level (dictionaries, arrays, etc.) where I haven't yet written
compatible bindings. For those APIs you can check out the document "[Using Fleece][FLEECE]".


[RUST]: https://www.rust-lang.org
[CBL]: https://www.couchbase.com/products/lite
[CBLDOCS]: https://docs.couchbase.com/couchbase-lite/current/introduction.html
[FLEECE]: https://github.com/couchbaselabs/fleece/wiki/Using-Fleece
[BINDGEN]: https://rust-lang.github.io/rust-bindgen/
[BINDGEN_INSTALL]: https://rust-lang.github.io/rust-bindgen/requirements.html
