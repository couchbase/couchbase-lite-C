#! /usr/bin/env python
# Run this to build the PyCBL native glue library
# See https://cffi.readthedocs.io/en/latest/index.html

from cffi import FFI
import shutil

SrcLibraryDir = "../../build/CBL_C/Build/Products/Debug/" # FIX: Xcode-specific
LibraryName = "couchbase_lite"
LibraryFilename = "lib" + LibraryName + ".dylib"    # FIX: Mac/iOS specific

ffibuilder = FFI()
ffibuilder.set_source("PyCBL",
   r""" // passed to the real C compiler,
        // contains implementation of things declared in cdef()
        #include <cbl/CouchbaseLite.h>
    """,
    libraries=[LibraryName],
    include_dirs=["../../include", "../../vendor/couchbase-lite-core/vendor/fleece/API"],
    library_dirs=["."],
    extra_link_args=["-rpath", "@loader_path"])         # FIX: Mac-only

ffibuilder.cdef("""
// Declarations that are shared between Python and C
// Careful, this supports only a subset of C syntax

void free(void *);

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
char* cbl_error_message(const CBLError*);

void cbl_setLogLevel(CBLLogLevel, CBLLogDomain);

typedef ... CBLRefCounted;
CBLRefCounted* cbl_retain(void*);
void cbl_release(void*);

typedef ... CBLDatabase;
typedef ... CBLDocument;
typedef ... CBLQuery;
typedef ... CBLResultSet;
typedef ... CBLReplicator;
typedef ... CBLListenerToken;

typedef struct {
    const char *directory;
    ...;
} CBLDatabaseConfiguration;

//////// CBLDatabase.h
bool cbl_databaseExists(const char* name, const char *inDirectory);
bool cbl_copyDB(const char* fromPath,
                const char* toName, 
                const CBLDatabaseConfiguration* config,
                CBLError*);
bool cbl_deleteDB(const char *name, 
                  const char *inDirectory,
                  CBLError*);
CBLDatabase* cbl_db_open(const char *name,
                         const CBLDatabaseConfiguration* config,
                         CBLError* error);
bool cbl_db_close(CBLDatabase*, CBLError*);
bool cbl_db_delete(CBLDatabase*, CBLError*);
bool cbl_db_compact(CBLDatabase*, CBLError*);
bool cbl_db_beginBatch(CBLDatabase*, CBLError*);
bool cbl_db_endBatch(CBLDatabase*, CBLError*);
const char* cbl_db_name(const CBLDatabase*);
const char* cbl_db_path(const CBLDatabase*);
uint64_t cbl_db_count(const CBLDatabase*);
uint64_t cbl_db_lastSequence(const CBLDatabase*);
CBLDatabaseConfiguration cbl_db_config(const CBLDatabase*);

//////// CBLDocument.h
typedef uint8_t CBLConcurrencyControl;
const CBLDocument* cbl_db_getDocument(const CBLDatabase* database,
                                      const char* docID);
const CBLDocument* cbl_db_saveDocument(CBLDatabase* db,
                                       CBLDocument* doc,
                                       CBLConcurrencyControl concurrency,
                                       CBLError* error);
bool cbl_doc_delete(const CBLDocument* document,
                    CBLConcurrencyControl concurrency,
                    CBLError* error);
bool cbl_db_deleteDocument(CBLDatabase* database,
                           const char* docID,
                           CBLError* error);
bool cbl_doc_purge(const CBLDocument* document,
                   CBLError* error);
bool cbl_db_purgeDocument(CBLDatabase* database,
                          const char* docID,
                          CBLError* error);
CBLDocument* cbl_db_getMutableDocument(CBLDatabase* database,
                                       const char* docID);
CBLDocument* cbl_doc_new(const char *docID);
CBLDocument* cbl_doc_mutableCopy(const CBLDocument* original);
const char* cbl_doc_id(const CBLDocument*);
uint64_t cbl_doc_sequence(const CBLDocument*);
FLDict cbl_doc_properties(const CBLDocument*);
FLMutableDict cbl_doc_mutableProperties(CBLDocument*);
char* cbl_doc_propertiesAsJSON(const CBLDocument*);
bool cbl_doc_setPropertiesAsJSON(CBLDocument*, const char *json, CBLError*);

//////// CBLQuery.h
CBLQuery* cbl_query_new(const CBLDatabase* db,
                        const char *jsonQuery, 
                        CBLError* error);
void cbl_query_setParameters(CBLQuery* query,
                             FLDict parameters);
void cbl_query_setParametersFromJSON(CBLQuery* query,
                                     const char* json);
CBLResultSet* cbl_query_execute(CBLQuery*, CBLError*);
FLSliceResult cbl_query_explain(CBLQuery*);
unsigned cbl_query_columnCount(CBLQuery*);
FLSlice cbl_query_columnName(CBLQuery*,
                             unsigned columnIndex);
bool cbl_results_next(CBLResultSet*);
FLValue cbl_results_column(CBLResultSet*,
                           unsigned column);
FLValue cbl_results_property(CBLResultSet*,
                             const char* property);
//CBLListenerToken* cbl_query_addListener(CBLQuery* query,
//                                        CBLQueryListener* listener,
//                                        void *context);
""")

if __name__ == "__main__":
    shutil.copy(SrcLibraryDir + LibraryFilename, ".")
    ffibuilder.compile(verbose=True)
