#!/bin/bash -e

PREFIX=""
WORKING_DIR="${1}"

if [[ $# > 1 ]]; then
  PREFIX="${2}"
fi

resolved_file=$(basename $(readlink -f libcblite.so))
pushd $WORKING_DIR
COMMAND="${PREFIX}objcopy --only-keep-debug $resolved_file tmp"
eval ${COMMAND}
COMMAND="find . -name \"*.a\" | xargs ${PREFIX}strip --strip-unneeded"
eval ${COMMAND}
rm libcblite.so*
mv tmp libcblite.so.sym
make -j8 cblite
COMMAND="${PREFIX}strip --strip-unneeded $resolved_file"
eval ${COMMAND}
COMMAND="${PREFIX}objcopy --add-gnu-debuglink=libcblite.so.sym $resolved_file"
eval ${COMMAND}
popd
