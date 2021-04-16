#! /bin/bash -e
#
# Xcode build script to generate CBL_Edition.h

# Input and Output file:
INPUT_FILE="$1"
OUTPUT_FILE="$2"

# Generated headers directory:
GEN_HEADERS_DIR="$SRCROOT/Xcode/generated_headers"
mkdir -p "$GEN_HEADERS_DIR"

# Generated header file:
GEN_HEADER_FILE="$GEN_HEADERS_DIR/CBL_Edition.h"

# Generate:
if [ "$CONFIGURATION" == "Debug_EE" ] || [ "$CONFIGURATION" == "Release_EE" ]; then
  cat "$INPUT_FILE" | sed 's/#cmakedefine/#define/g' > "$GEN_HEADER_FILE"
else
  cat "$INPUT_FILE" | sed 's/#cmakedefine\(.*\)/\/\* #undef\1 \*\//g' > "$GEN_HEADER_FILE"
fi

# Copy the generated header to the derived file directory:
cp "$GEN_HEADER_FILE" "$OUTPUT_FILE"
