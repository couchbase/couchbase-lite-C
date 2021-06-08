#!/bin/bash -ex

# This is the script for the official Couchbase build server.  Do not try to use it, it will only confuse you.
# You have been warned.

# Global define
PRODUCT=${1}
BLD_NUM=${2}
VERSION=${3}
EDITION=${4}

if [[ -z "${WORKSPACE}" ]]; then
    WORKSPACE=`pwd`
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PKG_CMD='zip -r'
PKG_TYPE='zip'
OS="ios"
BUILD_IOS_REL_TARGET='build_ios_release'
PROP_FILE=${WORKSPACE}/publish_ios.prop

if [ "${EDITION}" = "enterprise" ]; then
    $SCRIPT_DIR/../scripts/build_apple.sh --ee
else
    $SCRIPT_DIR/../scripts/build_apple.sh
fi

PACKAGE_NAME=${PRODUCT}-${OS}-${VERSION}-${BLD_NUM}-${EDITION}.${PKG_TYPE}
echo
echo  "=== Creating ${WORKSPACE}/${PACKAGE_NAME} package ==="
echo

${PKG_CMD} ${WORKSPACE}/${PACKAGE_NAME} ${WORKSPACE}/couchbase-lite-c/build_apple_out/CouchbaseLite.xcframework
RELEASE_IOS_PKG_NAME=${PACKAGE_NAME}

# Create Nexus publishing prop file
cd ${WORKSPACE}
echo "PRODUCT=${PRODUCT}"  >> ${PROP_FILE}
echo "BLD_NUM=${BLD_NUM}"  >> ${PROP_FILE}
echo "VERSION=${VERSION}" >> ${PROP_FILE}
echo "PKG_TYPE=${PKG_TYPE}" >> ${PROP_FILE}
echo "RELEASE_IOS_PKG_NAME=${RELEASE_IOS_PKG_NAME}" >> ${PROP_FILE}
echo
echo  "=== Created ${WORKSPACE}/${PROP_FILE} ==="
echo

cat ${PROP_FILE}
