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
VERSION=$(cat ${VS_VERSION_FILE} | cut -f1 -d-)
BLD_NUM=$(cat ${VS_VERSION_FILE} | cut -f2 -d-)

function download() {
    echo "Download Vector Search Framework ${VERSION}-${BLD_NUM} ..."
    curl -O http://latestbuilds.service.couchbase.com/builds/latestbuilds/couchbase-lite-vector-search/${VERSION}/${BLD_NUM}/${1}
}

ZIP_BASE_FILENAME="couchbase-lite-vector-search-${VERSION}-${BLD_NUM}"

if [ "${PLATFORM}" == "apple" ]
then
    rm -rf apple && mkdir -p apple
    pushd apple > /dev/null

    FILENAME="couchbase-lite-vector-search_xcframework_${VERSION}-${BLD_NUM}"
    ZIP_FILENAME="${FILENAME}.zip"
    download "${ZIP_FILENAME}"
    unzip "${ZIP_FILENAME}"

    cp CouchbaseLiteVectorSearch.xcframework/macos-arm64_x86_64/CouchbaseLiteVectorSearch.framework/Versions/Current/CouchbaseLiteVectorSearch CouchbaseLiteVectorSearch.dylib
    rm -rf "${ZIP_FILENAME}"

    popd > /dev/null
elif [ "${PLATFORM}" == "linux" ]
then
    rm -rf linux/x86_64 && mkdir -p linux/x86_64
    pushd linux/x86_64 > /dev/null

    ZIP_FILENAME="${ZIP_BASE_FILENAME}"-linux-x86_64.zip
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

    FILENAME="${ZIP_BASE_FILENAME}-windows-x86_64"
    ZIP_FILENAME="${FILENAME}.zip"
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

    ZIP_FILENAME="${ZIP_BASE_FILENAME}-android-x86_64.zip"
    download "${ZIP_FILENAME}"
    unzip "${ZIP_FILENAME}"

    mv lib/*.* .
    rm -rf lib
    rm -rf "${ZIP_FILENAME}"

    popd > /dev/null

    rm -rf android/arm64-v8a && mkdir -p android/arm64-v8a
    pushd android/arm64-v8a > /dev/null

    ZIP_FILENAME="${ZIP_BASE_FILENAME}-android-arm64-v8a.zip"
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
