#! /usr/bin/env python
# Run this to build the PyCBL native glue library
# See https://cffi.readthedocs.io/en/latest/index.html

from cffi import FFI
import shutil

SrcLibraryDir = "../../build/CBL_C/Build/Products/Debug/"
LibraryName = "couchbase_lite"

ffibuilder = FFI()
ffibuilder.set_source("PyCBL",
   r""" // passed to the real C compiler,
        // contains implementation of things declared in cdef()
        #include <cbl/CouchbaseLite.h>
    """,
    libraries=[LibraryName],
    include_dirs=["../../include", "../../vendor/couchbase-lite-core/vendor/fleece/API"],
    library_dirs=["."],
    extra_link_args=["-rpath", "@loader_path"])         # FIX: This is Mac-only

ffibuilder.cdef("""
// Declarations that are shared between Python and C
// Careful, this supports only a subset of C syntax

void free(void *);

//////// Fleece:
typedef ... * FLValue;         ///< A reference to a value of any type.
typedef ... * FLArray;         ///< A reference to an array value.
typedef ... * FLDict;          ///< A reference to a dictionary (map) value.
typedef ... * FLMutableArray;  ///< A reference to a mutable array.
typedef ... * FLMutableDict;   ///< A reference to a mutable dictionary.
typedef unsigned FLError;

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


""")

if __name__ == "__main__":
    shutil.copy(SrcLibraryDir + "lib" + LibraryName + ".dylib", ".")
    ffibuilder.compile(verbose=True)
