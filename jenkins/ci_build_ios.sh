#!/bin/bash -ex

# NOTE: This is for Couchbase internal CI usage.  
# This room is full of dragons, so you *will* get confused.  
# You have been warned.

# Global define (export 2 so they get used in the subscript as well)
PRODUCT=${1}
export BLD_NUM=${2}
export VERSION=${3}
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

ln -sf ${WORKSPACE}/couchbase-lite-c-ee/couchbase-lite-core-EE ${WORKSPACE}/couchbase-lite-c/vendor/couchbase-lite-core-EE

if [ "${EDITION}" = "enterprise" ]; then
    $SCRIPT_DIR/../scripts/build_apple.sh --ee
else
    $SCRIPT_DIR/../scripts/build_apple.sh
fi

PACKAGE_NAME=${PRODUCT}-${EDITION}-${VERSION}-${BLD_NUM}-${OS}.${PKG_TYPE}
echo
echo  "=== Creating ${WORKSPACE}/${PACKAGE_NAME} package ==="
echo

pushd ${WORKSPACE}/couchbase-lite-c/build_apple_out/
cp ${WORKSPACE}/product-texts/mobile/couchbase-lite/license/LICENSE_${EDITION}.txt LICENSE.txt
#notices.txt is produced by blackduck.
#It is not part of source tar, it is download to the workspace by a separate curl command by jenkins job.
if [[ -f ${WORKSPACE}/notices.txt ]]; then
    cp ${WORKSPACE}/notices.txt notices.txt
fi
${PKG_CMD} ${WORKSPACE}/${PACKAGE_NAME} CouchbaseLite.xcframework *.txt

RELEASE_IOS_PKG_NAME=${PACKAGE_NAME}
popd

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
