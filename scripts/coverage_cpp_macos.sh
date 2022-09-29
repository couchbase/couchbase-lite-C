#!/bin/bash -e

# Copyright 2022-Present Couchbase, Inc.
#
# Use of this software is governed by the Business Source License included in
# the file licenses/BSL-Couchbase.txt.  As of the Change Date specified in that
# file, in accordance with the Business Source License, use of this software
# will be governed by the Apache License, Version 2.0, included in the file
# licenses/APL2.txt.

# This script is for generating code coverage results for CPP API on macOS platform.

function usage 
{
  echo "Usage: ${0} [--ce] [--show] [--push]"
}

while [[ $# -gt 0 ]]
do
  key=${1}
  case $key in
      --ce)
      CE="Y"
      ;;
      --show)
      SHOW="Y"
      ;;
      --push)
      PUSH="Y"
      ;;
      *)
      usage
      exit 3
      ;;
  esac
  shift
done

if [ -n "$CE" ]
then
  BUILD_ENTERPRISE="OFF"
else
  BUILD_ENTERPRISE="ON"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")"; pwd)";
pushd $SCRIPT_DIR/.. > /dev/null

# If calling from a PR Validation :
if [[ -n "$CHANGE_ID" ]]; then
  git submodule update --init --recursive
  if [[ -z "$CE" ]]; then
    pushd vendor
    if [[ $CHANGE_TARGET == "master" ]]; then
      BRANCH="main"
    else
      BRANCH=$CHANGE_TARGET
    fi
    rm -rf couchbase-lite-c-ee couchbase-lite-core-EE
    git clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $BRANCH_NAME --recursive --depth 1 couchbase-lite-c-ee || \
      git clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $BRANCH --recursive --depth 1 couchbase-lite-c-ee
    mv couchbase-lite-c-ee/couchbase-lite-core-EE .
    popd
  fi
fi

mkdir -p build_coverage_cpp
pushd build_coverage_cpp > /dev/null
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_ENTERPRISE=${BUILD_ENTERPRISE} -DCODE_COVERAGE_ENABLED=ON ..    
core_count=`getconf _NPROCESSORS_ONLN`
make -j `expr $core_count + 1`

pushd test > /dev/null
./CBL_C_Tests -r list
popd > /dev/null

xcrun llvm-profdata merge -sparse test/default.profraw -o test/default.profdata

mkdir -p report

ARCH="x86_64"
FILE_INFO=`file test/CBL_C_Tests`
if [[ "$FILE_INFO" == *"arm64"* ]]; then
  ARCH="arm64"
fi

xcrun llvm-cov show -instr-profile=test/default.profdata -show-line-counts-or-regions -arch $ARCH \
 -output-dir=report -format="html" \
 -ignore-filename-regex="/cbl/.*\.h$" \
 -ignore-filename-regex="/test/*" \
 -ignore-filename-regex="/vendor/*" \
 test/CBL_C_Tests

xcrun llvm-cov export -instr-profile=test/default.profdata -arch $ARCH \
  -ignore-filename-regex="/cbl/.*\.h$" \
  -ignore-filename-regex="/test/*" \
  -ignore-filename-regex="/vendor/*" \
  test/CBL_C_Tests > report/coverage.json

if [[ -n "$SHOW" ]]; then
  open report/index.html
fi

if [[ -n "$PUSH" ]] && [[ -n "$CHANGE_ID" ]]; then
  echo "Pushing the coverage result to PR #$CHANGE_ID ..."
  python3 -m venv venv
  source venv/bin/activate 
  pip install -r $SCRIPT_DIR/push_coverage_results_requirements.txt
  python $SCRIPT_DIR/push_coverage_results.py -r report/coverage.json -n $CHANGE_ID
fi

echo "Done : The coverage report was generated at build_coverage_cpp/report"

popd > /dev/null
popd > /dev/null
