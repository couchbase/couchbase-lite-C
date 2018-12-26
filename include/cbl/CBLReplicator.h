//////// Couchbase Lite 2 C API Proposal -- DRAFT


#include "CBLBase.h"
#include "fleece/FLSlice.h"


// Configuration

typedef CBL_ENUM(uint8_t, CBLReplicatorType) { ... };

typedef struct {
    CBLDatabase* database;
    CBLEndpoint* endpoint;
    CBLReplicatorType replicatorType;
    bool continuous;
    CBLAuthenticator* authenticator;
    FLSlice pinnedServerCertificate;
    FLDict headers;
    const char **channels;          // i.e. ptr to C array of C strings, ending with NULL
    const char **documentIDs;
} CBLReplicatorConfiguration;


// Replicator

CBL_REFCOUNTED(CBLReplicator*, repl);

cbl_repl_new(CBLReplicatorConfiguration* _cblnonnull);

const CBLReplicatorConfiguration* cbl_repl_config(CBLReplicator* _cblnonnull);

void cbl_repl_start(CBLReplicator* _cblnonnull);
void cbl_repl_stop(CBLReplicator* _cblnonnull);

void cbl_repl_resetCheckpoint(CBLReplicator* _cblnonnull);


// Status

typedef CBL_ENUM(uint8_t, CBLReplicatorActivityLevel) { ... };

typedef struct {
    uint64_t completed, total;
} CBLReplicatorProgress;

typedef struct {
    CBLReplicatorActivityLevel activity;
    CBLReplicatorProgress progress;
    CBLError error;
} CBLReplicatorStatus;
    
const CBLReplicatorStatus* cbl_repl_status(CBLReplicator*);


// Change Listener

typedef void (*CBLReplicatorListener)(void *context, 
                                      CBLReplicator* _cblnonnull, 
                                      const CBLReplicatorStatus* _cblnonnull);

CBLListenerToken* cbl_repl_addListener(CBLReplicator* _cblnonnull,
                                       CBLReplicatorListener _cblnonnull, 
                                       void *context);