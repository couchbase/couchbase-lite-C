#!/bin/sh

# Rewrites quoted #include directives in the framework's public headers
# to use framework-style angle-bracket includes.
# e.g., #include "fleece/FLSlice.h" → #include <CouchbaseLite/FLSlice.h>

set -e

cd "$BUILT_PRODUCTS_DIR/$PUBLIC_HEADERS_FOLDER_PATH/"

sed -i '' -E 's/#include "([^"]*\/)?([^"]+)"/#include <CouchbaseLite\/\2>/' *
