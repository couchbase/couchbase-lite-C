#!/bin/bash -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
pushd $SCRIPT_DIR/..

NDK_DEFAULT_VERSION="21.4.7075529"
CMAKE_DEFAULT_VERSION="3.18.1"

mkdir -p build_android_out
mkdir -p build_android_x86
mkdir -p build_android_arm
mkdir -p build_android_x64
mkdir -p build_android_arm64

if [ -z "${ANDROID_SDK_ROOT}" ]; then
    echo "Error: ANDROID_SDK_ROOT not set, aborting..."
    exit 1
fi

if [ -z "${ANDROID_NDK_VERSION}" ]; then
    echo "ANDROID_NDK_VERSION not set, defaulting to ${NDK_DEFAULT_VERSION}"
    ANDROID_NDK_VERSION=${NDK_DEFAULT_VERSION}
fi

if [ -z "${ANDROID_CMAKE_VERSION}" ]; then
    echo "ANDROID_CMAKE_VERSION not set, defaulting to ${CMAKE_DEFAULT_VERSION}"
    ANDROID_CMAKE_VERSION=${CMAKE_DEFAULT_VERSION}
fi

function build_variant {
    $ANDROID_SDK_ROOT/cmake/$ANDROID_CMAKE_VERSION/bin/cmake -G Ninja \
	    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_SDK_ROOT/ndk/$ANDROID_NDK_VERSION/build/cmake/android.toolchain.cmake \
	    -DCMAKE_MAKE_PROGRAM=$ANDROID_SDK_ROOT/cmake/$ANDROID_CMAKE_VERSION/bin/ninja \
	    -DANDROID_NATIVE_API_LEVEL=22 \
	    -DANDROID_ABI=$1 \
	    -DCMAKE_INSTALL_PREFIX=`pwd`/../build_android_out \
	    ..

    $ANDROID_SDK_ROOT/cmake/$ANDROID_CMAKE_VERSION/bin/cmake --build . --target install
}

set -x
cd build_android_x86
build_variant x86

cd ../build_android_arm
build_variant armeabi-v7a

cd ../build_android_x64
build_variant x86_64

cd ../build_android_arm64
build_variant arm64-v8a
