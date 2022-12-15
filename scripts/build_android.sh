#!/bin/bash -e

function usage
{
  echo "Usage: ${0} [-a <x86,armeabi-v7a,x86_64,arm64-v8a>]"
}

while [[ $# -gt 0 ]]
do
  key=${1}
  case $key in
      -a)
      ARCHS=${2}
      shift
      ;;
      *)
      usage
      exit 3
      ;;
  esac
  shift
done

if [ -z "$ARCHS" ]
then
    BUILD_ARCHS=("x86" "armeabi-v7a" "x86_64" "arm64-v8a")
else
    BUILD_ARCHS=(`echo $ARCHS | tr ',' ' '`)
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
pushd $SCRIPT_DIR/..

NDK_DEFAULT_VERSION="23.1.7779620"
CMAKE_DEFAULT_VERSION="3.18.1"

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
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DCMAKE_INSTALL_PREFIX=`pwd`/../build_android_out \
        ..
    $ANDROID_SDK_ROOT/cmake/$ANDROID_CMAKE_VERSION/bin/cmake --build . --target install
}

# Create install directory:
mkdir -p build_android_out

# Build library for each archs:
echo "Architectures : ${BUILD_ARCHS[@]}"
for ARCH in "${BUILD_ARCHS[@]}"
  do
    echo "Build ${ARCH} ..."
    mkdir -p build_${ARCH}
    pushd build_${ARCH}
    build_variant ${ARCH}
    popd
    rm -rf build_${ARCH}
done

popd