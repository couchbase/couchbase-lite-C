#!/bin/sh

# Copies Fleece public headers into the framework's Headers/ directory,
# filtering out internal headers that shouldn't be exposed.

set -e

# Copy Fleece Headers:
cp vendor/couchbase-lite-core/vendor/fleece/API/fleece/* "$BUILT_PRODUCTS_DIR/$PUBLIC_HEADERS_FOLDER_PATH/"

# Remove internal headers:
cd "$BUILT_PRODUCTS_DIR/$PUBLIC_HEADERS_FOLDER_PATH/"
rm -f FLExpert.h
rm -f Expert.hh
