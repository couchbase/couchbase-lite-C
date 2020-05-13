#! /bin/bash -e
#
# This is an Xcode custom build script that merges additional static libraries into the static
# library built by a target.
#
# In the "Run Script Phase" text box, set the command to `source path/to/mergeIntoStaticLib.sh`.
# Then add the extra library paths to the "Input Files" list.
#
# Thanks to <http://geme.github.io/xcode/2016/10/27/BuildPhaseLoopOverInput.html>

# First concatenate the input file paths:
FILES=""
COUNTER=0
while [ $COUNTER -lt $SCRIPT_INPUT_FILE_COUNT ]; do
    tmp="SCRIPT_INPUT_FILE_$COUNTER"
    FILES="$FILES ${!tmp}"
    let COUNTER=COUNTER+1
done
echo "File list is: $FILES"

# Now use `libtool` to add those files:
cd $BUILT_PRODUCTS_DIR
mv $EXECUTABLE_PATH tmp.a
libtool -static -o $EXECUTABLE_PATH - tmp.a $FILES
rm tmp.a
