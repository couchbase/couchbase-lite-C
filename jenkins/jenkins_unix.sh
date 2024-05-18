#!/bin/bash -e

# Copyright 2022-Present Couchbase, Inc.
#
# Use of this software is governed by the Business Source License included in
# the file licenses/BSL-Couchbase.txt.  As of the Change Date specified in that
# file, in accordance with the Business Source License, use of this software
# will be governed by the Apache License, Version 2.0, included in the file
# licenses/APL2.txt.

# This script is for PR Validation. The script builds the binaries for macOS/linux and runs the tests.

if [ $CHANGE_TARGET == "master" ]; then
    BRANCH="main"
else
    BRANCH=$CHANGE_TARGET
fi

git submodule update --init --recursive
pushd vendor > /dev/null
rm -rf couchbase-lite-c-ee couchbase-lite-core-EE
git clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $BRANCH_NAME --recursive --depth 1 couchbase-lite-c-ee || \
    git clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $BRANCH --recursive --depth 1 couchbase-lite-c-ee
mv couchbase-lite-c-ee/couchbase-lite-core-EE .
popd > /dev/null

if [[ $OSTYPE == 'darwin'* ]]; then
  ./scripts/download_vector_search_extension.sh apple
else
  ./scripts/download_vector_search_extension.sh linux
fi

mkdir -p build
pushd build > /dev/null
cmake -DBUILD_ENTERPRISE=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=`pwd`/out ..
core_count=`getconf _NPROCESSORS_ONLN`
make -j `expr $core_count + 1`

pushd test > /dev/null
./CBL_C_Tests -r list
popd > /dev/null

popd > /dev/null

# Cleanup downloaded extension files
if [[ $OSTYPE == 'darwin'* ]]; then
  rm -rf test/extensions/apple
else
  rm -rf test/extensions/linux
fi
