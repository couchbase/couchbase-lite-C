//
//  CBLCheckpoint_Internal.hh
//  CBL_C
//
//  Created by Jens Alfke on 7/17/20.
//  Copyright © 2020 Couchbase. All rights reserved.
//

#include "CBLCheckpoint.h"
#include "CBLDatabase_Internal.hh"
#include "CBLReplicatorConfig.hh"
#include "c4Private.h"
#include "Checkpointer.hh"
#include "Checkpoint.hh"
#include "ReplicatorOptions.hh"
#include "InstanceCounted.hh"
#include "Logging.hh"
#include <chrono>

using namespace litecore;
using namespace fleece;


class CBLCheckpoint : public CBLRefCounted,
                      repl::Options,
                      public repl::Checkpointer,
                      public InstanceCountedIn<CBLCheckpoint>
{
public:
    CBLCheckpoint(CBLDatabase *db, const C4ReplicatorParameters &params, slice url)
    :repl::Options(params)
    ,repl::Checkpointer(*this, url)
    ,_db(db)
    { }

    void enableSave(repl::Checkpointer::duration interval,
                    CBLCheckpointSaveCallback callback,
                    void *context)
    {
        _callback = callback;
        _callbackContext = context;
        enableAutosave(interval, [this](alloc_slice json) {
            _jsonBeingSaved = json;
            CBLCheckpointSaveCallback callback = _callback;
            if (callback) {
                callback(_callbackContext, string(json).c_str());
            }
        });
    }

    void disableSave() {
        _callback = nullptr;
    }

    void writeLatest() {
        alloc_slice json = std::move(_jsonBeingSaved);
        _db->use([&](C4Database *c4db) {
            C4Error err;
            if (!write(c4db, json, &err))
                WarnError("Unable to save local checkpoint: %s", c4error_descriptionStr(err));
        });
    }

private:
    Retained<CBLDatabase> _db;
    std::atomic<CBLCheckpointSaveCallback> _callback = nullptr;
    void* _callbackContext;
    alloc_slice _jsonBeingSaved;
};


