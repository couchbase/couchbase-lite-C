#!/bin/bash -ex

# Global define
PRODUCT=${1}
BLD_NUM=${2}
VERSION=${3}
EDITION=${4}
OS=${5}
STRIP_PREFIX=${6}
TOOLCHAIN_FILE=${7}

if [[ -z "${WORKSPACE}" ]]; then
    WORKSPACE=`pwd`
fi

mkdir -p ${WORKSPACE}/build_release
PKG_CMD='tar czf'
PKG_TYPE='tar.gz'
PROP_FILE=${WORKSPACE}/publish.prop
project_dir=couchbase-lite-c
strip_dir=${project_dir}

echo VERSION=${VERSION}
# Global define end

ln -sf ${WORKSPACE}/couchbase-lite-c-ee/couchbase-lite-core-EE ${WORKSPACE}/couchbase-lite-c/vendor/couchbase-lite-core-EE

echo "====  Cross Building Release binary using ${TOOLCHAIN_FILE}  ==="
cd ${WORKSPACE}/build_release
cmake -DEDITION=${EDITION} -DCMAKE_INSTALL_PREFIX=`pwd`/install -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} ..
make -j8

echo "==== Stripping binary using ${STRIP-PREFIX}-strip"
${WORKSPACE}/couchbase-lite-c/jenkins/strip.sh ${strip_dir} ${STRIP_PREFIX}
make install

# package up the strip symbols
cp -rp ${strip_dir}/libCouchbaseLiteC.so.sym  ./install/

cd ${WORKSPACE}

PACKAGE_NAME=${PRODUCT}-${OS}-${VERSION}-${BLD_NUM}-${EDITION}.${PKG_TYPE}
echo
echo  "=== Creating ${WORKSPACE}/${PACKAGE_NAME} package ==="
echo

cd ${WORKSPACE}/build_release/install
${PKG_CMD} ${WORKSPACE}/${PACKAGE_NAME} include lib
SYMBOLS_RELEASE_PKG_NAME=${PRODUCT}-${OS}-${VERSION}-${BLD_NUM}-${EDITION}-'symbols'.${PKG_TYPE}
${PKG_CMD} ${WORKSPACE}/${SYMBOLS_RELEASE_PKG_NAME} libCouchbaseLiteC*.sym

cd ${WORKSPACE}

echo "PRODUCT=${PRODUCT}"  >> ${PROP_FILE}
echo "BLD_NUM=${BLD_NUM}"  >> ${PROP_FILE}
echo "VERSION=${VERSION}" >> ${PROP_FILE}
echo "PKG_TYPE=${PKG_TYPE}" >> ${PROP_FILE}
echo "RELEASE_PKG_NAME=${PACKAGE_NAME}" >> ${PROP_FILE}
echo "SYMBOLS_RELEASE_PKG_NAME=${SYMBOLS_RELEASE_PKG_NAME}" >> ${PROP_FILE}

echo
echo  "=== Created ${WORKSPACE}/${PROP_FILE} ==="
echo

cat ${PROP_FILE}
