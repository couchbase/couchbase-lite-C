#!/bin/bash -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

ANDROID_NDK_VERSION="21.4.7075529"
ANDROID_CMAKE_VERSION="3.18.1"
PKG_TYPE="zip"
PKG_CMD="zip -r"

function usage() {
    echo "Usage: $0 <version> <build num> <edition>"
    echo "Required env: ANDROID_HOME"
    echo "Optional env: WORKSPACE"
    exit 1
}

if [ "$#" -ne 3 ]; then
    usage
fi

if [ -z "$WORKSPACE" ]; then
    WORKSPACE=$(pwd)
fi

if [ -z "$ANDROID_HOME" ]; then
    usage
fi

VERSION="$1"
if [ -z "$VERSION" ]; then
    usage
fi

BLD_NUM="$2"
if [ -z "$BLD_NUM" ]; then
    usage
fi

EDITION="$3"
if [ -z "$EDITION" ]; then
    usage
fi

SDK_MGR="${ANDROID_HOME}/tools/bin/sdkmanager"
CMAKE_PATH="${ANDROID_HOME}/cmake/${ANDROID_CMAKE_VERSION}/bin"

echo " ======== Installing toolchain with CMake ${ANDROID_CMAKE_VERSION} and NDK ${ANDROID_NDK_VERSION} (this will accept the licenses!)"
yes | ${SDK_MGR} --licenses > /dev/null 2>&1
${SDK_MGR} --install "cmake;${ANDROID_CMAKE_VERSION}" > /dev/null
${SDK_MGR} --install "ndk;${ANDROID_NDK_VERSION}" > /dev/null

function build_variant {
    ${CMAKE_PATH}/cmake -G Ninja \
	    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_HOME/ndk/$ANDROID_NDK_VERSION/build/cmake/android.toolchain.cmake \
	    -DCMAKE_MAKE_PROGRAM=${CMAKE_PATH}/ninja \
	    -DANDROID_NATIVE_API_LEVEL=22 \
	    -DANDROID_ABI=$1 \
	    -DCMAKE_INSTALL_PREFIX=$(pwd)/../build_android_out \
	    ..

    ${CMAKE_PATH}/ninja install/strip
}

set -x
mkdir -p build_android_x86
cd build_android_x86
build_variant x86

mkdir -p ../build_android_arm
cd ../build_android_arm
build_variant armeabi-v7a

mkdir -p ../build_android_x64
cd ../build_android_x64
build_variant x86_64

mkdir -p ../build_android_arm64
cd ../build_android_arm64
build_variant arm64-v8a

PACKAGE_NAME="couchbase-lite-c-android-${VERSION}-${BLD_NUM}.${PKG_TYPE}"
echo
echo "=== Creating ${WORKSPACE}/${PACKAGE_NAME}"
echo

cd $(pwd)/../build_android_out
${PKG_CMD} ${WORKSPACE}/${PACKAGE_NAME} *

cd ${WORKSPACE}
PROP_FILE="${WORKSPACE}/publish_android.prop"
echo "PRODUCT=couchbase-lite-c" > ${PROP_FILE}
echo "BLD_NUM=${BLD_NUM}" >> ${PROP_FILE}
echo "VERSION=${VERSION}" >> ${PROP_FILE}
echo "PKG_TYPE=${PKG_TYPE}" >> ${PROP_FILE}
echo "RELEASE_PKG_NAME=${PACKAGE_NAME}" >> ${PROP_FILE}

PACKAGE_NAME="couchbase-lite-c-android-${VERSION}.${PKG_TYPE}"
echo
echo "=== Created ${PROP_FILE}"
echo

cat ${PROP_FILE}
