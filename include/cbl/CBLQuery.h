//////// Couchbase Lite 2 C API Proposal -- DRAFT


#include "CBLBase.h"


// Query

CBL_REFCOUNTED(CBLQuery*, query);

CBLQuery* cbl_query_new(CBLDatabase* _cblnonnull, 
                        const char *jsonQuery _cblnonnull, 
                        CBLError*);
    // NOTE: Queries are specified using the JSON syntax. 
    // IMO, a C version of the CBL 2 query-building API would be unbearably verbose,
    // and counter to the lightweight style of C.

void cbl_query_setParameters(CBLQuery* _cblnonnull, FLDict);

CBLResultSet* cbl_query_execute(CBLQuery* _cblnonnull, CBLError*);

char* cbl_query_explain(CBLQuery* _cblnonnull);


// Change Listener

typedef void (*CBLQueryListener)(void *context, 
                                 CBLQuery* _cblnonnull, 
                                 CBLResultSet*, 
                                 CBLError*);

CBLListenerToken* cbl_query_addListener(CBLQuery* _cblnonnull, 
                                        CBLQueryChangeListener* _cblnonnull,
                                        void *context);


// Result Set

CBL_REFCOUNTED(CBLResultSet*, results);

bool cbl_results_next(CBLResultSet* _cblnonnull);

FLDict cbl_results_properties(CBLResultSet* _cblnonnull);

FLArray cbl_results_columns(CBLResultSet* _cblnonnull);

    // NOTE: Yes, that's it. The Fleece API has almost everything needed to work with result
    // properties ... the only things missing are dates and blobs.
    // https://github.com/couchbaselabs/fleece/blob/master/API/fleece/Fleece.h