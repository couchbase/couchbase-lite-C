#!/bin/bash -e

#
# Download vector search extension for tests based on the vector specified in test/extensions/version.txt".
# The extension will be stored in the test/extensions folder.
# The script will not download the extensions if the extension of the specified version already exists.
#
# Note : Require Couchbase VPN in order download the extension.
#

function usage() {
    echo -e "Usage: $0 <platform: apple, linux, windows, android>"
    exit 1
}

if [ "$#" -ne 1 ]; then
    usage
fi

PLATFORM=$1
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
ROOT_DIR="${SCRIPT_DIR}/.."
EXTENSIONS_DIR="${ROOT_DIR}/test/extensions"

pushd "${EXTENSIONS_DIR}" > /dev/null
EXTENSIONS_DIR=`pwd`

VS_VERSION_FILE="${EXTENSIONS_DIR}/version.txt"
VERSION_NUMBER=$(cat ${VS_VERSION_FILE})

if [[ "$VERSION_NUMBER" == *"-"* ]]; then
  VERSION="${VERSION_NUMBER%-*}"
  BLD_NUM="${VERSION_NUMBER##*-}"
  XCFRAMEWORK_ZIP_FILENAME="couchbase-lite-vector-search_xcframework_${VERSION}-${BLD_NUM}.zip"
  BASE_ZIP_FILENAME="couchbase-lite-vector-search-${VERSION}-${BLD_NUM}"
  BASE_DOWNLOAD_URL="http://latestbuilds.service.couchbase.com/builds/latestbuilds/couchbase-lite-c/${VERSION}/${BLD_NUM}"
else
  VERSION="$VERSION_NUMBER"
  XCFRAMEWORK_ZIP_FILENAME="couchbase-lite-vector-search_xcframework_${VERSION}.zip"
  BASE_ZIP_FILENAME="couchbase-lite-vector-search-${VERSION}"
  BASE_DOWNLOAD_URL="https://packages.couchbase.com/releases/couchbase-lite-vector-search/${VERSION}"
fi

function download() {
    DOWNLOAD_URL="${BASE_DOWNLOAD_URL}/${1}"
    echo "Download Version Search from ${DOWNLOAD_URL}"
    curl -O ${DOWNLOAD_URL}
}

if [ "${PLATFORM}" == "apple" ]
then
    rm -rf apple && mkdir -p apple
    pushd apple > /dev/null

    download "${XCFRAMEWORK_ZIP_FILENAME}"
    unzip "${XCFRAMEWORK_ZIP_FILENAME}"
    cp CouchbaseLiteVectorSearch.xcframework/macos-arm64_x86_64/CouchbaseLiteVectorSearch.framework/Versions/Current/CouchbaseLiteVectorSearch CouchbaseLiteVectorSearch.dylib
    rm -rf "${XCFRAMEWORK_FILENAME}"

    popd > /dev/null
elif [ "${PLATFORM}" == "linux" ]
then
    rm -rf linux/x86_64 && mkdir -p linux/x86_64
    pushd linux/x86_64 > /dev/null

    ZIP_FILENAME="${BASE_ZIP_FILENAME}"-linux-x86_64.zip
    download "${ZIP_FILENAME}"
    unzip "${ZIP_FILENAME}"

    mv lib/*.* .
    rm -rf lib
    rm -rf "${ZIP_FILENAME}"

    popd > /dev/null
elif [ "${PLATFORM}" == "windows" ]
then
    rm -rf windows/x86_64 && mkdir -p windows/x86_64
    pushd windows/x86_64 > /dev/null

    ZIP_FILENAME="${BASE_ZIP_FILENAME}-windows-x86_64.zip"
    download "${ZIP_FILENAME}"
    unzip "${ZIP_FILENAME}"

    mv lib/*.* .
    mv bin/*.* .
    rm -rf lib && rm -rf bin
    rm -rf "${ZIP_FILENAME}"
    
    popd > /dev/null
elif [ "${PLATFORM}" == "android" ]
then
    rm -rf android/x86_64 && mkdir -p android/x86_64
    pushd android/x86_64 > /dev/null

    ZIP_FILENAME="${BASE_ZIP_FILENAME}-android-x86_64.zip"
    download "${ZIP_FILENAME}"
    unzip "${ZIP_FILENAME}"

    mv lib/*.* .
    rm -rf lib
    rm -rf "${ZIP_FILENAME}"

    popd > /dev/null

    rm -rf android/arm64-v8a && mkdir -p android/arm64-v8a
    pushd android/arm64-v8a > /dev/null

    ZIP_FILENAME="${BASE_ZIP_FILENAME}-android-arm64-v8a.zip"
    download "${ZIP_FILENAME}"
    unzip "${ZIP_FILENAME}"
    rm -rf "${ZIP_FILENAME}"

    mv lib/*.* .
    rm -rf lib

    popd > /dev/null
else
    usage
fi

popd > /dev/null
