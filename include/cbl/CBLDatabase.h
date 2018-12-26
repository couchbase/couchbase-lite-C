#pragma once
#include "CBLBase.h"


// Database Configuration

#ifdef COUCHBASE_ENTERPRISE
/** Encryption algorithms. */
typedef CBL_ENUM(uint32_t, CBLEncryptionAlgorithm) {
    kCBLEncryptionNone = 0,      ///< No encryption (default)
    kCBLEncryptionAES256,        ///< AES with 256-bit key
};

/** Encryption key sizes (in bytes). */
typedef CBL_ENUM(uint64_t, CBLEncryptionKeySize) {
    kCBLEncryptionKeySizeAES256 = 32,
};

/** Encryption key specified in a CBLDatabaseConfiguration. */
typedef struct CBLEncryptionKey {
    CBLEncryptionAlgorithm algorithm;
    uint8_t bytes[32];
} CBLEncryptionKey;
#endif

typedef struct {
    const char *directory;
#ifdef COUCHBASE_ENTERPRISE
    CBLEncryptionKey encryptionKey;
#endif
} CBLDatabaseConfiguration;


// Static methods

bool cbl_databaseExists(const char* _cblnonnull name, const char *inDirectory);

bool cbl_copyDB(const char* _cblnonnull fromPath,
                const char* _cblnonnull toName, 
                const CBLDatabaseConfiguration*, 
                CBLError*);

bool cbl_deleteDatabase(const char _cblnonnull *name, 
                        const char *inDirectory, 
                        CBLError*);


// Database

CBL_REFCOUNTED(CBLDatabase*, db);

CBLDatabase* cbl_db_open(const char *name _cblnonnull, 
                         const CBLDatabaseConfiguration*, 
                         CBLError*);

bool cbl_db_close(CBLDatabase* _cblnonnull, CBLError*);

const char* cbl_db_name(CBLDatabase* _cblnonnull);
const char* cbl_db_path(CBLDatabase* _cblnonnull);
uint64_t cbl_db_count(CBLDatabase* _cblnonnull);
const CBLDatabaseConfiguration* cbl_db_config(CBLDatabase* _cblnonnull);

bool cbl_db_compact(CBLDatabase* _cblnonnull, CBLError*);

bool cbl_db_delete(CBLDatabase* _cblnonnull, CBLError*);

bool cbl_db_beginBatch(CBLDatabase* _cblnonnull, CBLError*);
bool cbl_db_endBatch(CBLDatabase* _cblnonnull, CBLError*);
    // NOTE: Without blocks/lambdas, an `inBatch` function that takes a callback would be 
    // quite awkward to use!
    // Instead, expose the begin/end functions and trust the developer to balance them.


// Listeners

typedef void (*CBLDatabaseListener)(void *context, CBLDatabase* _cblnonnull,
                                    const char **docIDs);
    // NOTE: docIDs is a C array of C strings, ending with a NULL.
                                    
typedef void (*CBLDocumentListener)(void *context, CBLDatabase* _cblnonnull, 
                                    const char *docID);

CBLListenerToken* cbl_db_addListener(CBLDatabase* _cblnonnull, 
                                     CBLDatabaseListener _cblnonnull,
                                     void *context);
                                     
CBLListenerToken* cbl_db_addDocumentListener(CBLDatabase* _cblnonnull,
                                             const char* _cblnonnull docID,
                                             CBLDocumentListener _cblnonnull, 
                                             void *context);
