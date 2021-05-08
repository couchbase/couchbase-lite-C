#!/bin/bash -e
#
# Builds Couchbase Lite for C on Unix-type systems.
#
# Prerequisites: GCC 7+ or Clang; CMake 3.9+; ICU libraries (see README.md)
# Built libraries will appear in build_cmake subdirectory
# Test binary will be build_cmake/test/CBL_C_Tests
#
# Mac developers may prefer to use the Xcode project in the Xcode/ directory.

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR"
mkdir -p build_cmake
cd build_cmake

echo '*** BEGINNING DEBUG BUILD ***'

core_count=`getconf _NPROCESSORS_ONLN`
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j `expr $core_count + 1`
