#! /bin/bash -e
#
# NOTE: c2nim can be obtained from https://github.com/nim-lang/c2nim
#
# This script does not replace the binding source files cbl.nim and cbl.nim;
# instead it creates new ones, cbl-new.nim and fl-new.nim.
# The binding files started out as output from this tool, but have been hand-edited to work
# around limitations of c2nim. The files produced by this script will _not_ work.
#
# The best procedure for updating bindings would be to run this script before and after changing
# the C headers, compare the old and new generated files, and apply those diffs by hand to the
# bindings.

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR"

ROOT="../../../../.."
INCLUDE="$ROOT/include/cbl"
FLEECE="$ROOT/vendor/couchbase-lite-core/vendor/fleece/API/fleece"

c2nim --concat -o:fl-new.nim cbl.c2nim --dynlib:'"CouchbaseLite.dylib"' --nep1  "$FLEECE/FLSlice.h" "$FLEECE/Fleece.h"

c2nim --concat -o:cbl-new.nim cbl.c2nim --dynlib:'"CouchbaseLite.dylib"' --nep1  "$INCLUDE"/CBLBase.h "$INCLUDE"/CBLDatabase.h "$INCLUDE"/CBLDocument.h "$INCLUDE"/CBLBlob.h "$INCLUDE"/CBLLog.h "$INCLUDE"/CBLQuery.h "$INCLUDE"/CBLReplicator.h
