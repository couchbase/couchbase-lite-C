#!/bin/bash -e

# Copyright 2022-Present Couchbase, Inc.
#
# Use of this software is governed by the Business Source License included in
# the file licenses/BSL-Couchbase.txt.  As of the Change Date specified in that
# file, in accordance with the Business Source License, use of this software
# will be governed by the Apache License, Version 2.0, included in the file
# licenses/APL2.txt.

# This script is for PR Validation. The script builds the iOS framework from the xcode project 
# without running the tests. The tests will be run as part on the mac validation.

if [ $CHANGE_TARGET == "master" ]; then
    BRANCH="main"
else
    BRANCH=$CHANGE_TARGET
fi

git submodule update --init --recursive
pushd vendor
rm -rf couchbase-lite-c-ee couchbase-lite-core-EE
git clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $BRANCH_NAME --recursive --depth 1 couchbase-lite-c-ee || \
    git clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $BRANCH --recursive --depth 1 couchbase-lite-c-ee
mv couchbase-lite-c-ee/couchbase-lite-core-EE .
popd

xcodebuild -project CBL_C.xcodeproj -configuration Debug-EE -derivedDataPath ios -scheme "CBL_C Framework" -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO
