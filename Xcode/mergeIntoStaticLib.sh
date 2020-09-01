#! /bin/bash -e
#
# This script merges the LiteCore static libraries into the static lib built by the CBL_C target.
# (The real work is done by the makefile.)

cd $BUILT_PRODUCTS_DIR
make -f $SRCROOT/Xcode/mergeIntoStaticLib-Makefile.txt
