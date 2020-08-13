#!/bin/bash

set -e
shopt -s extglob dotglob

function build_xcode {
    xcodebuild -project CBL_C.xcodeproj -configuration Debug-EE -derivedDataPath ios -scheme "CBL_C Framework" -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO
}


if [ ! -z $CHANGE_TARGET ]; then
    BRANCH=$CHANGE_TARGET
fi

if ! [ -x "$(command -v git)" ]; then
  echo 'Error: git is not installed.' >&2
  exit 1
fi

git submodule update --init --recursive
pushd vendor
git clone ssh://git@github.com/couchbase/couchbase-lite-core-EE --branch $BRANCH --recursive --depth 1 couchbase-lite-core-EE
popd

unameOut="$(uname -s)"
case "${unameOut}" in
    # Build XCode project on mac because it has stricter warnings
    Darwin*)    build_xcode;;
esac

ulimit -c unlimited # Enable crash dumps
mkdir -p build
pushd build
cmake -DBUILD_ENTERPRISE=ON ..
make -j8
pushd test
./CBL_C_Tests -r list
