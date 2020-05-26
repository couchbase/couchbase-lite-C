#! /bin/bash -e
#
# Xcode build script to generate the CBL-C exported symbols list.

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR"

RESULT="$DERIVED_FILE_DIR/CBL.exp"

cat CBL.exp Fleece.exp >"$RESULT"

if [ "$CONFIGURATION" == "Debug_EE" ] || [ "$CONFIGURATION" == "Release_EE" ]; then
    cat CBL_EE.exp >>"$RESULT"
fi
