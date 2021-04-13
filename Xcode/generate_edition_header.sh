#! /bin/bash -e
#
# Xcode build script to generate the CBL-C CBL_Edition.h

# Input file:
INPUT_FILE="$SRCROOT/include/cbl/CBL_Edition.h.in"

# Output directory:
OUTPUT_DIR="$SRCROOT/Xcode/generated_headers"
OUTPUT_FILE="$OUTPUT_DIR/CBL_Edition.h"

mkdir -p "$OUTPUT_DIR"

if [ "$CONFIGURATION" == "Debug_EE" ] || [ "$CONFIGURATION" == "Release_EE" ]; then
  cat "$INPUT_FILE" | sed 's/#cmakedefine/#define/g' > "$OUTPUT_DIR/CBL_Edition.h"
else
  cat "$INPUT_FILE" | sed 's/#cmakedefine\(.*\)/\/\* #undef\1 \*\//g' > "$OUTPUT_FILE"
fi

# Copy to the derived file directory for building and installing:
cp "$OUTPUT_FILE" "$DERIVED_FILE_DIR/CBL_Edition.h"
