#!/bin/bash

PREFIX=""
WORKING_DIR="${1}"

if [[ $# > 1 ]]; then
  PREFIX="${2}"
fi

pushd $WORKING_DIR
COMMAND="find . -name \"*.a\" | xargs ${PREFIX}strip --strip-unneeded"
eval ${COMMAND}
rm libCouchbaseLiteC.so
make -j8 CouchbaseLiteC
COMMAND="${PREFIX}objcopy --only-keep-debug libCouchbaseLiteC.so libCouchbaseLiteC.so.sym"
eval ${COMMAND}
COMMAND="${PREFIX}strip --strip-unneeded libCouchbaseLiteC.so"
eval ${COMMAND}
COMMAND="${PREFIX}objcopy --add-gnu-debuglink=libCouchbaseLiteC.so.sym libCouchbaseLiteC.so"
eval ${COMMAND}
popd
