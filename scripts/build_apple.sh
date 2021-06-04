#!/bin/bash -e

function usage
{
  echo "Usage: ${0} [--ee]"
}

while [[ $# -gt 0 ]]
do
  key=${1}
  case $key in
    --ee)
    EE="Y"
    ;;
    *)
    usage
    exit 3
    ;;
  esac
  shift
done

if [ -z "$EE" ]
then
  echo "Build for Community Edition ..."
  CONFIGURATION="Release"
else
  echo "Build for Enterprise Edition ..."
  CONFIGURATION="Release_EE"
fi

if [ -z "$VERSION" ]; then
  echo "VERSION environment variable not set, using 0.0.0"
  XCODE_BUILD_VERSION="CBL_VERSION_STRING=0.0.0"
else
  XCODE_BUILD_VERSION="CBL_VERSION_STRING=${VERSION}"
fi

if [ -z "$BLD_NUM" ]; then
  echo "BLD_NUM environment variable not set, using 0"
  XCODE_BUILD_NUMBER="CBL_BUILD_NUMBER=0"
else
  XCODE_BUILD_NUMBER="CBL_BUILD_NUMBER=${BLD_NUM}"
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

pushd $SCRIPT_DIR/.. > /dev/null

# Use absolute path to workaround an xcodebuild bug that cannot recognize the dSym files 
# using their relative paths when creating the xcframework file.
OUTPUT_DIR=`pwd`/build_apple_out
BUILD_DIR="$OUTPUT_DIR/build"
mkdir -p "$BUILD_DIR"

FRAMEWORK_NAME="CouchbaseLite"

# this will be used to collect all destination framework path with `-framework`
# to include them in `-create-xcframework`
FRAMEWORK_PATH_ARGS=()

# arg1 = target destination for which the archive is built for. E.g., "generic/platform=iOS"
function xcarchive
{
  PLATFORM_NAME=${1}
  DESTINATION=${2}
  
  echo "Archiving for ${DESTINATION}..."
  ARCHIVE_PATH=${BUILD_DIR}/${PLATFORM_NAME}
  xcodebuild archive \
    -scheme "CBL_C Framework" \
    -configuration "${CONFIGURATION}" \
    -destination "${DESTINATION}" \
    ${XCODE_BUILD_VERSION} ${XCODE_BUILD_NUMBER} \
    -archivePath "${ARCHIVE_PATH}/${FRAMEWORK_NAME}.xcarchive" \
    "ONLY_ACTIVE_ARCH=NO" "BITCODE_GENERATION_MODE=bitcode" \
    "CODE_SIGNING_REQUIRED=NO" "CODE_SIGN_IDENTITY=" \
    "SKIP_INSTALL=NO"
  
  FRAMEWORK_PATH_ARGS+=("-framework "${ARCHIVE_PATH}/${FRAMEWORK_NAME}.xcarchive/Products/Library/Frameworks/${FRAMEWORK_NAME}.framework" \
    -debug-symbols "${ARCHIVE_PATH}/${FRAMEWORK_NAME}.xcarchive/dSYMs/${FRAMEWORK_NAME}.framework.dSYM"")
  echo "Finished archiving ${DESTINATION}."
}

# Build and archive binary for each platform:
xcarchive "iossim" "generic/platform=iOS Simulator"
xcarchive "ios" "generic/platform=iOS"

# Until required, don't bundle mac since we provide another package for it
# xcarchive "macos" "generic/platform=macOS"

# Create xcframework
echo "Creating XCFramework...: ${FRAMEWORK_PATH_ARGS}"
xcodebuild -create-xcframework \
    -output "${OUTPUT_DIR}/${FRAMEWORK_NAME}.xcframework" \
    ${FRAMEWORK_PATH_ARGS[*]}

# Remove build directory
rm -rf ${BUILD_DIR}
echo "Finished creating XCFramework. Output at "${OUTPUT_DIR}/${FRAMEWORK_NAME}.xcframework""
popd  > /dev/null
