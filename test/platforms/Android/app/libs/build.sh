#!/bin/bash -e

# Copyright 2022-Present Couchbase, Inc.
#
# Use of this software is governed by the Business Source License included in
# the file licenses/BSL-Couchbase.txt.  As of the Change Date specified in that
# file, in accordance with the Business Source License, use of this software
# will be governed by the Apache License, Version 2.0, included in the file
# licenses/APL2.txt.

SCRIPT_DIR=`dirname $0`

pushd "${SCRIPT_DIR}/../../../../.." > /dev/null

./scripts/build_android.sh

rm -rf test/platforms/Android/app/libs/libcblite
cp -r build_android_out test/platforms/Android/app/libs/libcblite

popd > /dev/null