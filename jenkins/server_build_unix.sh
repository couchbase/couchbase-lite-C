#!/bin/bash -ex

# Global define
PRODUCT=${1}
BLD_NUM=${2}
VERSION=${3}
EDITION=${4}

if [[ -z "${WORKSPACE}" ]]; then
    WORKSPACE=`pwd`
fi

mkdir -p ${WORKSPACE}/build_release

case "${OSTYPE}" in
    darwin*)  OS="macosx"
              PKG_CMD='zip -r'
              PKG_TYPE='zip'
              PROP_FILE=${WORKSPACE}/publish.prop
              ;;
    linux*)   OS="linux"
              PKG_CMD='tar czf'
              PKG_TYPE='tar.gz'
              PROP_FILE=${WORKSPACE}/publish.prop
              OS_NAME=`lsb_release -is`
              if [[ "$OS_NAME" != "CentOS" ]]; then
                  echo "Error: Unsupported Linux distro $OS_NAME"
                  exit 2
              fi

              OS_VERSION=`lsb_release -rs`
              if [[ ! $OS_VERSION =~ ^7.* ]]; then
                  echo "Error: Unsupported CentOS version $OS_VERSION"
                  exit 3
              fi;;
    *)        echo "unknown: $OSTYPE"
              exit 1;;
esac

project_dir=couchbase-lite-c
strip_dir=${project_dir}
macosx_lib="libCouchbaseLiteC.dylib"

echo VERSION=${VERSION}
# Global define end

echo "====  Building macosx/linux Release binary  ==="
cd ${WORKSPACE}/build_release
cmake -DEDITION=${EDITION} -DCMAKE_INSTALL_PREFIX=`pwd`/install -DCMAKE_BUILD_TYPE=MinSizeRel ..
make -j8
if [[ ${OS} == 'linux' ]]; then
    ${WORKSPACE}/couchbase-lite-c/jenkins/strip.sh ${strip_dir}
else
    pushd ${project_dir}
    dsymutil ${macosx_lib} -o libCouchbaseLiteC.dylib.dSYM
    strip -x ${macosx_lib}
    popd
fi

make install
if [[ ${OS} == 'macosx' ]]; then
    # package up the strip symbols
    cp -rp ${strip_dir}/libCouchbaseLiteC.dylib.dSYM  ./install/
else
    # package up the strip symbols
    cp -rp ${strip_dir}/libCouchbaseLiteC.so.sym  ./install/

    # copy C++ stdlib, etc to output
    libstdcpp=`g++ --print-file-name=libstdc++.so`
    libstdcppname=`basename "$libstdcpp"`
    libgcc_s=`gcc --print-file-name=libgcc_s.so`
    libgcc_sname=`basename "$libgcc_s"`

    cp -p "$libstdcpp" "./install/lib/$libstdcppname"
    ln -s "$libstdcppname" "./install/lib/${libstdcppname}.6"
    cp -p "${libgcc_s}" "./install/lib"
fi

if [[ -z ${SKIP_TESTS} ]] && [[ ${EDITION} == 'enterprise' ]]; then
    cd ${WORKSPACE}/build_release/${project_dir}/test
    ./CBL_C_Tests || exit 1
fi

cd ${WORKSPACE}
ln -sf ${WORKSPACE}/couchbase-lite-c-ee/couchbase-lite-core-EE ${WORKSPACE}/couchbase-lite-c/vendor/couchbase-lite-core-EE

PACKAGE_NAME=${PRODUCT}-${OS}-${VERSION}-${BLD_NUM}-${EDITION}.${PKG_TYPE}
echo
echo  "=== Creating ${WORKSPACE}/${PACKAGE_NAME} package ==="
echo

cd ${WORKSPACE}/build_release/install
# Create separate symbols pkg
if [[ ${OS} == 'macosx' ]]; then
    ${PKG_CMD} ${WORKSPACE}/${PACKAGE_NAME} include lib
    SYMBOLS_RELEASE_PKG_NAME=${PRODUCT}-${OS}-${VERSION}-${BLD_NUM}-${EDITION}-'symbols'.${PKG_TYPE}
    ${PKG_CMD} ${WORKSPACE}/${SYMBOLS_RELEASE_PKG_NAME}  libCouchbaseLiteC.dylib.dSYM
else # linux
    ${PKG_CMD} ${WORKSPACE}/${PACKAGE_NAME} include lib
    SYMBOLS_RELEASE_PKG_NAME=${PRODUCT}-${OS}-${VERSION}-${BLD_NUM}-${EDITION}-'symbols'.${PKG_TYPE}
    ${PKG_CMD} ${WORKSPACE}/${SYMBOLS_RELEASE_PKG_NAME} libCouchbaseLiteC*.sym
fi
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
