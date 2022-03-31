#!/bin/bash -e

SCRIPT_DIR=`dirname $0`
PROJECT_DIR="${SCRIPT_DIR}/.."

EXP_DIR="${PROJECT_DIR}/src/exports/generated"
CBL_HEADER_DIR="${PROJECT_DIR}/include/cbl"
FL_HEADER_DIR="${PROJECT_DIR}/vendor/couchbase-lite-core/vendor/fleece/API/fleece"

PRIVATE_EXPORTED_SYMBOLS="\
CBLDatabase_AddChangeDetailListener,\
CBLDatabase_DeleteDocumentByID,\
CBLDatabase_LastSequence,\
CBLDocument_CanonicalRevisionID,\
CBLDocument_Generation,\
CBLError_GetCaptureBacktraces,\
CBLError_SetCaptureBacktraces,\
CBLLog_BeginExpectingExceptions,\
CBLLog_EndExpectingExceptions,\
FLErrorDomain"

echo ""
echo "** Check Apple exp files **"

$SCRIPT_DIR/check_exp.py \
    --exp "${EXP_DIR}/*.exp" \
    --header "${CBL_HEADER_DIR}/*.h" \
    --header "${FL_HEADER_DIR}/*.h" \
    --exclude_header "${FL_HEADER_DIR}/*Expert.h*" \
    --exclude_exp_symbols "${PRIVATE_EXPORTED_SYMBOLS}" \
    --exclude_header_symbols "CBL_Init"

echo ""
echo "** Check Windows def files **"

$SCRIPT_DIR/check_exp.py \
    --exp "${EXP_DIR}/*.def" \
    --header "${CBL_HEADER_DIR}/*.h" \
    --header "${FL_HEADER_DIR}/*.h" \
    --exclude_header "${FL_HEADER_DIR}/*Expert.h*" \
    --exclude_header "${FL_HEADER_DIR}/*CoreFoundation.h" \
    --exclude_exp_symbols "${PRIVATE_EXPORTED_SYMBOLS}" \
    --exclude_header_symbols "CBL_Init"

echo ""
echo "** Check Linux gnu files **"

$SCRIPT_DIR/check_exp.py \
    --exp "${EXP_DIR}/*.gnu" \
    --exclude_exp "${EXP_DIR}/*Android.gnu" \
    --header "${CBL_HEADER_DIR}/*.h" \
    --header "${FL_HEADER_DIR}/*.h" \
    --exclude_header "${FL_HEADER_DIR}/*Expert.h*" \
    --exclude_header "${FL_HEADER_DIR}/*CoreFoundation.h" \
    --exclude_exp_symbols "${PRIVATE_EXPORTED_SYMBOLS}" \
    --exclude_header_symbols "CBL_Init"

echo ""
echo "** Check Android gnu files **"

$SCRIPT_DIR/check_exp.py \
    --exp "${EXP_DIR}/*Android.gnu" \
    --header "${CBL_HEADER_DIR}/*.h" \
    --header "${FL_HEADER_DIR}/*.h" \
    --exclude_header "${FL_HEADER_DIR}/*Expert.h*" \
    --exclude_header "${FL_HEADER_DIR}/*CoreFoundation.h" \
    --exclude_exp_symbols "${PRIVATE_EXPORTED_SYMBOLS}" \
