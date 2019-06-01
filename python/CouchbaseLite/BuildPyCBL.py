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


# Run this to build the PyCBL native glue library
# See https://cffi.readthedocs.io/en/latest/index.html

from cffi import FFI
import shutil
import argparse


# Mac settings are defaults, but overrideable on command-line
MAC_NAME = "couchbase_lite" # cmake uses "CouchBaseLiteC' instead
MAC_DIR = "../../build/CBL_C/Build/Products/Debug/"
MAC_LINK_FLAGS = "-rpath @loader_path"

CDEF = """
// Declarations that are shared between Python and C
// Careful, this supports only a subset of C syntax

void free(void *);
typedef long time_t;

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

void CBL_SetLogLevel(CBLLogLevel, CBLLogDomain);
void CBL_Log(CBLLogDomain, CBLLogLevel, const char *format, ...);

typedef ... CBLRefCounted;
CBLRefCounted* CBL_Retain(void*);
void CBL_Release(void*);

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

void CBLListener_Remove(CBLListenerToken*);

//////// CBLDatabase.h
bool CBL_DatabaseExists(const char* name, const char *inDirectory);
bool CBL_CopyDatabase(const char* fromPath,
                const char* toName,
                const CBLDatabaseConfiguration* config,
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
time_t CBLDatabase_NextDocExpiration(CBLDatabase*);
int64_t CBLDatabase_PurgeExpiredDocuments(CBLDatabase* db, CBLError* error);

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
time_t CBLDatabase_GetDocumentExpiration(CBLDatabase* db,
                                         const char *docID,
                                         CBLError* error);
bool CBLDatabase_SetDocumentExpiration(CBLDatabase* db,
                                       const char *docID,
                                       time_t expiration,
                                       CBLError* error);
CBLDocument* CBLDatabase_GetMutableDocument(CBLDatabase* database,
                                       const char* docID);
CBLDocument* CBLDocument_New(const char *docID);
CBLDocument* CBLDocument_MutableCopy(const CBLDocument* original);
const char* CBLDocument_ID(const CBLDocument*);
uint64_t CBLDocument_Sequence(const CBLDocument*);
FLDict CBLDocument_Properties(const CBLDocument*);
FLMutableDict CBLDocument_MutableProperties(CBLDocument*);
char* CBLDocument_PropertiesAsJSON(const CBLDocument*);
bool CBLDocument_SetPropertiesAsJSON(CBLDocument*, const char *json, CBLError*);

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
FLDict CBLQuery_Parameters(CBLQuery* query);
void CBLQuery_SetParameters(CBLQuery* query,
                             FLDict parameters);
void CBLQuery_SetParametersAsJSON(CBLQuery* query,
                                     const char* json);
CBLResultSet* CBLQuery_Execute(CBLQuery*, CBLError*);
FLSliceResult CBLQuery_Explain(CBLQuery*);
unsigned CBLQuery_ColumnCount(CBLQuery*);
FLSlice CBLQuery_ColumnName(CBLQuery*,
                             unsigned columnIndex);
bool CBLResultSet_Next(CBLResultSet*);
FLValue CBLResultSet_ValueAtIndex(CBLResultSet*, unsigned index);
FLValue CBLResultSet_ValueForKey(CBLResultSet*, const char* key);

typedef void (*CBLQueryChangeListener)(void *context,
                                       CBLQuery* query);
extern "Python" void queryListenerCallback(void *context, const CBLQuery *query);
CBLListenerToken* CBLQuery_AddChangeListener(CBLQuery* query,
                                        CBLQueryChangeListener listener,
                                        void *context);
"""

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="build python bindings")
    parser.add_argument('-n', '--name', default=MAC_NAME, help="Library name")
    parser.add_argument('-d', '--dir', default=MAC_DIR, help="Directory containing shared library file")
    parser.add_argument('-l', '--link_flags', default=MAC_LINK_FLAGS, help="Linker flags")
    args = parser.parse_args()

    ffibuilder = FFI()
    extra_link_args = args.link_flags.split()
    ffibuilder.cdef(CDEF)
    ffibuilder.set_source("_PyCBL", r""" // passed to the real C compiler,
        // contains implementation of things declared in cdef()
        #include <cbl/CouchbaseLite.h>
    """,
    libraries=[args.name],
    include_dirs=["../../include", "../../vendor/couchbase-lite-core/vendor/fleece/API"],
    library_dirs=[args.dir],
    extra_link_args=extra_link_args)

    ffibuilder.compile(verbose=True)
