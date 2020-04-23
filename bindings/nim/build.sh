#! /bin/bash -e
#
# NOTE: c2nim can be obtained from https://github.com/nim-lang/c2nim

SCRIPT_DIR=`dirname $0`
cd "$SCRIPT_DIR"

INCLUDE="../../include/cbl"
FLEECE="../../vendor/couchbase-lite-core/vendor/fleece/API/fleece"

c2nim --concat -o:Fleece-new.nim cbl.c2nim --dynlib:'"CouchbaseLite.dylib"' --nep1  "$FLEECE/FLSlice.h" "$FLEECE/Fleece.h"

c2nim --concat -o:CouchbaseLite-new.nim cbl.c2nim --dynlib:'"CouchbaseLite.dylib"' --nep1  "$INCLUDE"/CBLBase.h "$INCLUDE"/CBLDatabase.h "$INCLUDE"/CBLDocument.h "$INCLUDE"/CBLBlob.h "$INCLUDE"/CBLLog.h "$INCLUDE"/CBLQuery.h "$INCLUDE"/CBLReplicator.h