#! /bin/bash -e

PY_SRC="$SRCROOT/python/CouchbaseLite"
PY_DST="$TARGET_BUILD_DIR/python/CouchbaseLite"

# TEMP!! Fixes compatibility issue with distutils
export MACOSX_DEPLOYMENT_TARGET=10.14

# Create and populate the Python build directory
rm -rf "$PY_DST"
mkdir -p "$PY_DST"
cd "$PY_DST"
cp "$PY_SRC"/*.py .

/usr/local/bin/python3 "$SRCROOT/python/BuildPyCBL.py" \
    --srcdir "$SRCROOT" \
    --libdir "$TARGET_BUILD_DIR"

echo "Python package created at $PY_DST"
