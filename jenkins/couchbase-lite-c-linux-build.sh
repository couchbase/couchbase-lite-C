#!/bin/bash -ex

# This is the master entry point script for the generic
# couchbase-lite-c-linux-build job. It does the build by calling to
# either the x86_64 .sh or the ARM python scripts, then performs the
# packaging step the same for either platform.


function chk_set {
    var=$1
    # ${!var} is a little-known bashism that says "expand $var and then
    # use that as a variable name and expand it"
    if [[ -z "${!var}" ]]; then
        echo "\$${var} must be set!"
        exit 1
    fi
}

chk_set PRODUCT
chk_set VERSION
chk_set BLD_NUM
chk_set EDITION
chk_set ARCH
chk_set WORKSPACE

# Path to directory containing this shell script
script_dir=$(dirname $(readlink -e -- "${BASH_SOURCE}"))

# Convenience function for reading config file
cfgvar() {
    jq -r "$1" < ${script_dir}/linux-package-config.json
}

# Download notices file for inclusion in package
curl -LO https://raw.githubusercontent.com/couchbase/product-metadata/master/${PRODUCT}/blackduck/${VERSION}/notices.txt

# Cross-compile based on desired ARCH
BUILD_OS=$(cfgvar .build_arch_config.${ARCH}.build_os)
TARGET_OSNAME=$(cfgvar .build_arch_config.${ARCH}.target_osname)
STRIP_PREFIX=$(cfgvar .build_arch_config.${ARCH}.strip_prefix)
TOOLCHAIN=$(cfgvar .build_arch_config.${ARCH}.toolchain)

python3 -u ${script_dir}/ci_cross_build.py \
    ${PRODUCT} ${BLD_NUM} ${VERSION} ${EDITION} \
    ${BUILD_OS} ${TARGET_OSNAME} ${STRIP_PREFIX} \
    ${script_dir}/../cmake/${TOOLCHAIN}.cmake \
|| exit 1

rm -rf build
git clone https://github.com/couchbase/build.git
cd build/scripts/jenkins/lite-c/

# Create package for each desired target OS
BASE_DEPS="libc6,libatomic1,libgcc1,zlib1g"
if [ "${EDITION}" = "enterprise" ]; then
    EDITION_INFIX=
else
    EDITION_INFIX="-community"
fi

for TARGET_DEB in $(cfgvar ".build_arch_config.${ARCH}.target_debs[]"); do
    EXTRA_DEPS=$(cfgvar .package_distro_config.\"${TARGET_DEB}\".extra_deps)
    DEPENDENCIES="${BASE_DEPS},${EXTRA_DEPS}"

    ./package-deb.rb ${WORKSPACE}/build_release/libcblite-${VERSION} libcblite ${EDITION} ${VERSION}-${BLD_NUM} ${ARCH} ${DEPENDENCIES}
    cp build/deb/libcblite${EDITION_INFIX}_${VERSION}-${BLD_NUM}_${ARCH}.deb ${WORKSPACE}/libcblite-${EDITION}_${VERSION}-${BLD_NUM}-${TARGET_DEB}_${ARCH}.deb
    rm -rf build

    ./package-deb.rb ${WORKSPACE}/build_release/libcblite-${VERSION} libcblite-dev ${EDITION} ${VERSION}-${BLD_NUM} ${ARCH}
    cp build/deb/libcblite-dev${EDITION_INFIX}_${VERSION}-${BLD_NUM}_${ARCH}.deb ${WORKSPACE}/libcblite-dev-${EDITION}_${VERSION}-${BLD_NUM}-${TARGET_DEB}_${ARCH}.deb
    rm -rf build
done
