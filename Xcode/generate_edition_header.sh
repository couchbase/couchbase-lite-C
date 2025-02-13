#! /bin/bash -e
#
# Xcode build script to generate CBL_Edition.h

OUTPUT_FILE="$1"

if [ "$CONFIGURATION" == "Debug_EE" ] || [ "$CONFIGURATION" == "Release_EE" ]; then
    BUILD_ENTERPRISE="ON"
else
    BUILD_ENTERPRISE="OFF"
fi

PATH=$PATH:/opt/homebrew/bin

# Run cmake to generate CBL_Edition.h:
GEN_OUTPUT_DIR="${SRCROOT}/Xcode"
cmake -DVERSION="${CBL_VERSION_STRING}" -DOUTPUT_DIR="${GEN_OUTPUT_DIR}" -DBLD_NUM="${CBL_BUILD_NUMBER}" -DBUILD_ENTERPRISE="${BUILD_ENTERPRISE}" -P "${SRCROOT}/cmake/generate_edition.cmake"

# Copy the generated header to the derived file directory:
cp "${GEN_OUTPUT_DIR}/generated_headers/public/cbl/CBL_Edition.h" "${OUTPUT_FILE}"
