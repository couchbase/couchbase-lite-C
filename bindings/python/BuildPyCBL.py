#! /usr/bin/env python3
#
# BuildPyCBL.py
#
# Copyright (c) 2019 Couchbase, Inc All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


# Run this to build the PyCBL native glue library, in the current directory
# (Argument handling is at the end of the script)

import argparse
import os.path
import platform
import shutil
from cffi import FFI


# Assume we are running this from bindings/python/CouchbaseLite/
DEFAULT_SRC_DIR    = "../../../"

# Paths relative to the source root (--srcdir)
CBL_INCLUDE_DIR    = "/include"
FLEECE_INCLUDE_DIR = "/vendor/couchbase-lite-core/vendor/fleece/API"

# CMake settings are defaults, but overrideable on command-line
DEFAULT_LIBRARIES = "CouchbaseLiteC z"
DEFAULT_LIBRARY_DIR  = DEFAULT_SRC_DIR + "build_cmake/"

# Extra linker arguments -- platform-specific
DEFAULT_LINK_ARGS = ""
if platform.system() == "Darwin":
    DEFAULT_LINK_ARGS = "-rpath @loader_path"  # Look for CBL dylib in same dir as bindings lib


def BuildLibrary(sourceDir, python_includedir, libdir, libraries, extra_link_args):
    # when cross-compiling, use python headers for target rather than build system
    include_dirs = [sourceDir+CBL_INCLUDE_DIR, sourceDir+FLEECE_INCLUDE_DIR]
    if python_includedir:
        include_dirs.insert(0, python_includedir)

    # python 3.5 distutils breaks on Linux with absolute library path,
    # so make sure it is relative path
    libdir = os.path.relpath(libdir)

    # Copy the CBL library here:
    libpath = libdir + "/libCouchbaseLiteC"
    if platform.system() == "Darwin":
        libpath += ".dylib"
    else:
        libpath += ".so"
    shutil.copy(libpath, ".")

    # CFFI stuff -- see https://cffi.readthedocs.io/en/latest/index.html

    # This is passed to the real C compiler and should include the declarations of
    # the symbols declared in cdef()
    cHeaderSource = r"""#include <cbl/CouchbaseLite.h>"""

    ffibuilder = FFI()
    ffibuilder.cdef(CDeclarations())
    ffibuilder.set_source(
        "_PyCBL",       # Module name
        cHeaderSource,
        libraries=libraries,
        include_dirs=include_dirs,
        library_dirs=["."],
        extra_link_args=extra_link_args)
    ffibuilder.compile(verbose=True)

    os.remove("_PyCBL.c")
    os.remove("_PyCBL.o")


def CDeclarations():
    return r"""
// Declarations that are shared between Python and C
// (Careful, this supports only a subset of C syntax)
// Needs to be updated whenever public C API changes!

void free(void *);
typedef int64_t CBLTimestamp;

//////// Fleece (slices):
typedef struct {const void *buf; size_t size;} FLSlice;
typedef struct {const void *buf; size_t size;} FLSliceResult;
typedef FLSlice FLHeapSlice;
typedef FLSlice FLString;
typedef FLSliceResult FLStringResult;

bool FLSlice_Equal(FLSlice a, FLSlice b);
int FLSlice_Compare(FLSlice, FLSlice);
FLSliceResult FLSliceResult_Retain(FLSliceResult);
void FLSliceResult_Release(FLSliceResult);
FLSliceResult FLSlice_Copy(FLSlice);

//////// Fleece (values):
typedef ... * FLValue;         ///< A reference to a value of any type.
typedef ... * FLArray;         ///< A reference to an array value.
typedef ... * FLDict;          ///< A reference to a dictionary (map) value.
typedef ... * FLMutableArray;  ///< A reference to a mutable array.
typedef ... * FLMutableDict;   ///< A reference to a mutable dictionary.
typedef ... * FLSlot;
typedef unsigned FLError;

typedef enum {
    kFLUndefined = -1,  ///< Type of a NULL pointer, i.e. no such value, like JSON `undefined`. Also the type of a value created by FLEncoder_WriteUndefined().
    kFLNull = 0,        ///< Equivalent to a JSON 'null'
    kFLBoolean,         ///< A `true` or `false` value
    kFLNumber,          ///< A numeric value, either integer or floating-point
    kFLString,          ///< A string
    kFLData,            ///< Binary data (no JSON equivalent)
    kFLArray,           ///< An array of values
    kFLDict             ///< A mapping of strings to values
} FLValueType;

FLValueType FLValue_GetType(FLValue);
bool FLValue_IsInteger(FLValue);
bool FLValue_IsUnsigned(FLValue);
bool FLValue_IsDouble(FLValue);
bool FLValue_AsBool(FLValue);
int64_t FLValue_AsInt(FLValue);
uint64_t FLValue_AsUnsigned(FLValue);
float FLValue_AsFloat(FLValue);
double FLValue_AsDouble(FLValue);
FLString FLValue_AsString(FLValue);
FLArray FLValue_AsArray(FLValue);
FLDict FLValue_AsDict(FLValue);

typedef struct { ...; } FLArrayIterator;
uint32_t FLArray_Count(FLArray);
FLValue FLArray_Get(FLArray, uint32_t index);
void FLArrayIterator_Begin(FLArray, FLArrayIterator*);
FLValue FLArrayIterator_GetValue(const FLArrayIterator*);
FLValue FLArrayIterator_GetValueAt(const FLArrayIterator*, uint32_t offset);
uint32_t FLArrayIterator_GetCount(const FLArrayIterator*);
bool FLArrayIterator_Next(FLArrayIterator*);

typedef struct { ...; } FLDictIterator;
uint32_t FLDict_Count(FLDict);
FLValue FLDict_Get(FLDict, FLSlice keyString);
void FLDictIterator_Begin(FLDict, FLDictIterator*);
FLString FLDictIterator_GetKeyString(const FLDictIterator*);
FLValue FLDictIterator_GetValue(const FLDictIterator*);
uint32_t FLDictIterator_GetCount(const FLDictIterator* );
bool FLDictIterator_Next(FLDictIterator*);
void FLDictIterator_End(FLDictIterator*);


//////// CBLBase.h
typedef uint32_t CBLErrorDomain;
typedef uint8_t CBLLogDomain;
typedef uint8_t CBLLogLevel;

typedef struct {
    CBLErrorDomain domain;      ///< Domain of errors; a namespace for the `code`.
    int32_t code;               ///< Error code, specific to the domain. 0 always means no error.
    int32_t internal_info;
} CBLError;
char* CBLError_Message(const CBLError*);
FLSliceResult CBLError_Message_s(const CBLError*);

void CBL_Log(CBLLogDomain domain, CBLLogLevel level, const char *format, ...);
void CBL_Log_s(CBLLogDomain domain, CBLLogLevel level, FLSlice message);
CBLLogLevel CBLLog_ConsoleLevel(void);
void CBLLog_SetConsoleLevel(CBLLogLevel);
bool CBLLog_WillLogToConsole(CBLLogDomain domain, CBLLogLevel level);

typedef ... CBLRefCounted;
CBLRefCounted* CBL_Retain(void*);
void CBL_Release(void*);

typedef ... CBLBlob;
typedef ... CBLDatabase;
typedef ... CBLDocument;
typedef ... CBLQuery;
typedef ... CBLResultSet;
typedef ... CBLReplicator;
typedef ... CBLListenerToken;

typedef enum {
    kCBLDatabase_Create        = 1,  ///< Create the file if it doesn't exist
    kCBLDatabase_ReadOnly      = 2,  ///< Open file read-only
    kCBLDatabase_NoUpgrade     = 4,  ///< Disable upgrading an older-version database
} CBLDatabaseFlags;

typedef struct {
    const char *directory;
    CBLDatabaseFlags flags;
    ...;
} CBLDatabaseConfiguration;

typedef struct {
    FLString directory;
    CBLDatabaseFlags flags;
    ...;
} CBLDatabaseConfiguration_s;

void CBLListener_Remove(CBLListenerToken*);


//////// CBLBlob.h
bool CBL_IsBlob(FLDict);
const CBLBlob* CBLBlob_Get(FLDict blobDict);
uint64_t CBLBlob_Length(const CBLBlob*);
const char* CBLBlob_Digest(const CBLBlob*);
const char* CBLBlob_ContentType(const CBLBlob*);
FLDict CBLBlob_Properties(const CBLBlob*);
FLSliceResult CBLBlob_LoadContent(const CBLBlob*, CBLError *outError);

typedef ... CBLBlobReadStream;
CBLBlobReadStream* CBLBlob_OpenContentStream(const CBLBlob*, CBLError *outError);
int CBLBlobReader_Read(CBLBlobReadStream* stream, void *dst, size_t maxLength, CBLError *outError);
void CBLBlobReader_Close(CBLBlobReadStream*);

CBLBlob* CBLBlob_CreateWithData(const char *contentType, FLSlice contents);
CBLBlob* CBLBlob_CreateWithData_s(FLString contentType, FLSlice contents);

typedef ... CBLBlobWriteStream;
CBLBlobWriteStream* CBLBlobWriter_New(CBLDatabase *db, CBLError *outError);
void CBLBlobWriter_Close(CBLBlobWriteStream*);
bool CBLBlobWriter_Write(CBLBlobWriteStream* writer, const void *data, size_t length, CBLError *outError);
CBLBlob* CBLBlob_CreateWithStream(const char *contentType, CBLBlobWriteStream* writer);
CBLBlob* CBLBlob_CreateWithStream_s(FLString contentType, CBLBlobWriteStream* writer);

void FLSlot_SetBlob(FLSlot slot, CBLBlob* blob);


//////// CBLDatabase.h
bool CBL_DatabaseExists(const char* name, const char *inDirectory);
bool CBL_DatabaseExists_s(FLString name, FLString inDirectory);
bool CBL_CopyDatabase(const char* fromPath,
                const char* toName,
                const CBLDatabaseConfiguration* config,
                CBLError*);
bool CBL_CopyDatabase_s(FLString fromPath,
                        FLString toName,
                        const CBLDatabaseConfiguration_s* config,
                        CBLError*);
bool CBL_DeleteDatabase(const char *name,
                  const char *inDirectory,
                  CBLError*);
CBLDatabase* CBLDatabase_Open(const char *name,
                         const CBLDatabaseConfiguration* config,
                         CBLError* error);
bool CBLDatabase_Close(CBLDatabase*, CBLError*);
bool CBLDatabase_Delete(CBLDatabase*, CBLError*);
bool CBLDatabase_Compact(CBLDatabase*, CBLError*);
bool CBLDatabase_BeginBatch(CBLDatabase*, CBLError*);
bool CBLDatabase_EndBatch(CBLDatabase*, CBLError*);
const char* CBLDatabase_Name(const CBLDatabase*);
const char* CBLDatabase_Path(const CBLDatabase*);
uint64_t CBLDatabase_Count(const CBLDatabase*);
CBLDatabaseConfiguration CBLDatabase_Config(const CBLDatabase*);

typedef void (*CBLDatabaseChangeListener)(void *context,
                                     const CBLDatabase* db,
                                     unsigned numDocs,
                                     const char **docIDs);
extern "Python" void databaseListenerCallback(void *context, const CBLDatabase* db,
                                              unsigned numDocs, const char **docIDs);
CBLListenerToken* CBLDatabase_AddChangeListener(const CBLDatabase* db,
                                     CBLDatabaseChangeListener listener,
                                     void *context);


//////// CBLDocument.h
typedef uint8_t CBLConcurrencyControl;
const CBLDocument* CBLDatabase_GetDocument(const CBLDatabase* database,
                                      const char* docID);
const CBLDocument* CBLDatabase_GetDocument_s(const CBLDatabase* database,
                                      FLString docID);
const CBLDocument* CBLDatabase_SaveDocument(CBLDatabase* db,
                                       CBLDocument* doc,
                                       CBLConcurrencyControl concurrency,
                                       CBLError* error);
bool CBLDocument_Delete(const CBLDocument* document,
                    CBLConcurrencyControl concurrency,
                    CBLError* error);
bool CBLDocument_Purge(const CBLDocument* document,
                   CBLError* error);
bool CBLDatabase_PurgeDocumentByID(CBLDatabase* database,
                          const char* docID,
                          CBLError* error);
bool CBLDatabase_PurgeDocumentByID_s(CBLDatabase* database,
                                     FLString docID,
                                     CBLError* error);
CBLTimestamp CBLDatabase_GetDocumentExpiration(CBLDatabase* db,
                                         const char *docID,
                                         CBLError* error);
CBLTimestamp CBLDatabase_GetDocumentExpiration_s(CBLDatabase* db,
                                                 FLSlice docID,
                                                 CBLError* error);
bool CBLDatabase_SetDocumentExpiration(CBLDatabase* db,
                                       const char *docID,
                                       CBLTimestamp expiration,
                                       CBLError* error);
bool CBLDatabase_SetDocumentExpiration_s(CBLDatabase* db,
                                         FLSlice docID,
                                         CBLTimestamp expiration,
                                         CBLError* error);
CBLDocument* CBLDatabase_GetMutableDocument(CBLDatabase* database,
                                       const char* docID);
CBLDocument* CBLDatabase_GetMutableDocument_s(CBLDatabase* database,
                                              FLString docID);
CBLDocument* CBLDocument_New(const char *docID);
CBLDocument* CBLDocument_New_s(FLString docID);
CBLDocument* CBLDocument_MutableCopy(const CBLDocument* original);
const char* CBLDocument_ID(const CBLDocument*);
uint64_t CBLDocument_Sequence(const CBLDocument*);
FLDict CBLDocument_Properties(const CBLDocument*);
FLMutableDict CBLDocument_MutableProperties(CBLDocument*);
char* CBLDocument_PropertiesAsJSON(const CBLDocument*);
bool CBLDocument_SetPropertiesAsJSON(CBLDocument*, const char *json, CBLError*);
bool CBLDocument_SetPropertiesAsJSON_s(CBLDocument*, FLSlice json, CBLError*);

typedef void (*CBLDocumentChangeListener)(void *context,
                                          const CBLDatabase* db,
                                          const char *docID);
CBLListenerToken* CBLDatabase_AddDocumentChangeListener(const CBLDatabase* db,
                                                        const char* docID,
                                                        CBLDocumentChangeListener listener,
                                                        void *context);
extern "Python" void documentListenerCallback(void *context, const CBLDatabase*, const char *docID);

//////// CBLQuery.h
typedef enum {
    kCBLJSONLanguage,
    kCBLN1QLLanguage
} CBLQueryLanguage;
CBLQuery* CBLQuery_New(const CBLDatabase* db,
                       CBLQueryLanguage language,
                       const char *queryString,
                       int *outErrorPos,
                       CBLError* error);
CBLQuery* CBLQuery_New_s(const CBLDatabase* db,
                         CBLQueryLanguage language,
                         FLString queryString,
                         int *outErrorPos,
                         CBLError* error);
FLDict CBLQuery_Parameters(CBLQuery* query);
void CBLQuery_SetParameters(CBLQuery* query,
                             FLDict parameters);
void CBLQuery_SetParametersAsJSON(CBLQuery* query, const char* json);
bool CBLQuery_SetParametersAsJSON_s(CBLQuery* query, FLString json);
CBLResultSet* CBLQuery_Execute(CBLQuery*, CBLError*);
FLSliceResult CBLQuery_Explain(CBLQuery*);
unsigned CBLQuery_ColumnCount(CBLQuery*);
FLSlice CBLQuery_ColumnName(CBLQuery*,
                             unsigned columnIndex);
bool CBLResultSet_Next(CBLResultSet*);
FLValue CBLResultSet_ValueAtIndex(CBLResultSet*, unsigned index);
FLValue CBLResultSet_ValueForKey(CBLResultSet*, const char* key);
FLValue CBLResultSet_ValueForKey_s(const CBLResultSet*, FLString key);
FLArray CBLResultSet_RowArray(const CBLResultSet*);
FLDict CBLResultSet_RowDict(const CBLResultSet*);
CBLQuery* CBLResultSet_GetQuery(const CBLResultSet *rs);

typedef void (*CBLQueryChangeListener)(void *context,
                                       CBLQuery* query);
extern "Python" void queryListenerCallback(void *context, const CBLQuery *query);
CBLListenerToken* CBLQuery_AddChangeListener(CBLQuery* query,
                                        CBLQueryChangeListener listener,
                                        void *context);

typedef enum {
    kCBLValueIndex,         ///< An index that stores property or expression values
    kCBLFullTextIndex       ///< An index of strings, that enables searching for words with `MATCH`
} CBLIndexType;

typedef struct {
    CBLIndexType type;
    const char* keyExpressionsJSON;
    bool ignoreAccents;
    const char* language;
} CBLIndexSpec;

typedef struct {
    CBLIndexType type;
    FLString keyExpressionsJSON;
    bool ignoreAccents;
    FLString language;
} CBLIndexSpec_s;

bool CBLDatabase_CreateIndex(CBLDatabase *db,
                             const char* name,
                             CBLIndexSpec,
                             CBLError *outError);
bool CBLDatabase_CreateIndex_s(CBLDatabase *db,
                               FLString name,
                               CBLIndexSpec_s,
                               CBLError *outError);
bool CBLDatabase_DeleteIndex(CBLDatabase *db,
                             const char *name,
                             CBLError *outError);
FLMutableArray CBLDatabase_IndexNames(CBLDatabase *db);
"""


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="build Couchbase Lite Python bindings")
    parser.add_argument('--srcdir', default=DEFAULT_SRC_DIR,
                        help="Source root directory")
    parser.add_argument('--python_includedir', default='',
                        help="Directory containing python headers (specify for cross-compiling)")
    parser.add_argument('--libdir', default=DEFAULT_LIBRARY_DIR,
                        help="Directory containing CBL libraries")
    parser.add_argument('--libs', default=DEFAULT_LIBRARIES,
                        help="Library names")
    parser.add_argument('--link_flags', default=DEFAULT_LINK_ARGS,
                        help="Linker flags")
    args = parser.parse_args()

    linkFlags = None
    if args.link_flags != None:
        linkFlags = args.link_flags.split()

    BuildLibrary(args.srcdir, args.python_includedir, args.libdir, args.libs.split(), linkFlags)
