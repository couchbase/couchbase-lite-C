#! /bin/bash -e
#
# Xcode build script to generate the CBL-C exported symbols list.

cd "$SRCROOT/src/exports/"

TEMP_FILE="$DERIVED_FILE_DIR/exports.txt"
RESULT="$DERIVED_FILE_DIR/CBL.exp"

echo "Generating $RESULT"

cat CBL_Exports.txt Fleece_Exports.txt Fleece_Apple_Exports.txt >"$TEMP_FILE"

if [ "$CONFIGURATION" == "Debug_EE" ] || [ "$CONFIGURATION" == "Release_EE" ]; then
    cat CBL_EE_Exports.txt >>"$TEMP_FILE"
fi

awk '/^[A-Za-z_]/ { print "_" $0; next }' <"$TEMP_FILE" >"$RESULT"
