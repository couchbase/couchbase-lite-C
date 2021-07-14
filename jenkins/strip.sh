#!/bin/bash

PREFIX=""
WORKING_DIR="${1}"

if [[ $# > 1 ]]; then
  PREFIX="${2}"
fi

pushd $WORKING_DIR
COMMAND="find . -name \"*.a\" | xargs ${PREFIX}strip --strip-unneeded"
eval ${COMMAND}
rm libcblite.so*
make -j8 cblite
COMMAND="${PREFIX}objcopy --only-keep-debug libcblite.so libcblite.so.sym"
eval ${COMMAND}
COMMAND="${PREFIX}strip --strip-unneeded libcblite.so"
eval ${COMMAND}
COMMAND="${PREFIX}objcopy --add-gnu-debuglink=libcblite.so.sym libcblite.so"
eval ${COMMAND}
popd
