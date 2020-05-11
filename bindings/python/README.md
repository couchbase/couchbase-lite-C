# Python Bindings For Couchbase Lite

10 May 2020

This directory contains a [Couchbase Lite][CBL] API binding for Python 3.

It's still in early development. It's incomplete, has only had some informal testing, and that
only on macOS.

## Building

**_"Some assembly required..."_**

You first need to build Couchbase Lite For C (the root of this repo) with CMake, by running the
`build.sh` script in the repo root directory. That will produce the shared library.

    $ cd ../..
    $ ./build.sh

Then you can build the Python binding library, first installing Python's CFFI package if you haven't
already:

    $ pip install cffi
    $ cd bindings/python
    $ ./build.sh

Now try the rudimentary tests:

    $ test/test.sh

You can look at the test code in `test/test.py` for examples of how to use the API.

## Learning

If you're not already familiar with Couchbase Lite, you'll want to start by reading through its
[official documentation][CBLDOCS].

The Python API is mostly method-for-method compatible with the languages documented there, except
down at the document property level (dictionaries, arrays, etc.) where I haven't yet written
compatible bindings. For those APIs you can check out the document "[Using Fleece][FLEECE]".

## Using

[CBL]: https://www.couchbase.com/products/lite
[CBLDOCS]: https://docs.couchbase.com/couchbase-lite/current/introduction.html
[FLEECE]: https://github.com/couchbaselabs/fleece/wiki/Using-Fleece
