# Nim Language Bindings For Couchbase Lite

10 May 2020

This directory is a package containing a [Couchbase Lite][CBL] API binding for the [Nim
language][NIM].

It's still in early development. It's incomplete, has only had some informal testing, and that
only on macOS. Also I (Jens) am a newbie at Nim and may not be doing things the write way.
Feedback gratefully accepted.

## Building

**_"Some assembly required..."_**

You first need to build Couchbase Lite For C (the root of this repo) with CMake, by running the
`build.sh` script in the repo root directory. That will produce the shared library.

    $ cd ../..
    $ ./build.sh

Symlink or copy the library into this directory.

    $ cd bindings/nim
    $ ln -s ../../build_cmake/libCouchbaseLiteC.dylib .

After that, you can use [Nimble][NIMBLE] to build and run:

    $ nimble test

When compiling on Linux system remember to set `LD_LIBRARY_PATH` to point to the
directory where `libCouchbaseLiteC.so` exists. If you followed the above steps
this would look like this:

    $ LD_LIBRARY_PATH=. nimble test

Without the symlink you can also do:

    $ LD_LIBRARY_PATH=../../build_cmake nimble test

The supplied `CouchbaseLite.nimble` file is used to allow Nimble to run the
tests. If you want to try the bindings out you can create a `<filename>.nim`
file in this directory and add `bin = @["<filename>"]` to the
`CouchbaseLite.nimble` file. You can now run `nimble build` or `nimble run` to
build or build and run your file with the bindings.

## Learning

I've copied the doc-comments from the C API into the Nim files. But Couchbase Lite is fairly
complex, so if you're not already familiar with it, you'll want to start by reading through
the [official documentation][CBLDOCS].

The Nim API is mostly method-for-method compatible with the languages documented there, except
down at the document property level (dictionaries, arrays, etc.) where I haven't yet written
compatible bindings. For those APIs you can check out the document "[Using Fleece][FLEECE]".

## Using

[NIM]: https://nim-lang.org/
[CBL]: https://www.couchbase.com/products/lite
[NIMBLE]: https://github.com/nim-lang/nimble#readme
[CBLDOCS]: https://docs.couchbase.com/couchbase-lite/current/introduction.html
[FLEECE]: https://github.com/couchbaselabs/fleece/wiki/Using-Fleece
