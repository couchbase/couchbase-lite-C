#!/bin/bash -e

# Copyright 2022-Present Couchbase, Inc.
#
# Use of this software is governed by the Business Source License included in
# the file licenses/BSL-Couchbase.txt.  As of the Change Date specified in that
# file, in accordance with the Business Source License, use of this software
# will be governed by the Apache License, Version 2.0, included in the file
# licenses/APL2.txt.

# Xcode build script to copy the CBL-C exported symbols list.

SRC_DIR="${SRCROOT}/src/exports/generated"

if [ "${CONFIGURATION}" == "Debug_EE" ] || [ "${CONFIGURATION}" == "Release_EE" ]; then
    cp "${SRC_DIR}/CBL_EE.exp" "$1"
else
    cp "${SRC_DIR}/CBL.exp" "$1"
fi
