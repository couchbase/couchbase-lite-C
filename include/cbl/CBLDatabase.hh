//
// CBLDatabase.hh
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once
#include "CBLBase.hh"
#include "CBLDatabase.h"
#include <functional>
#include <vector>

namespace cbl {
    class Document;
    class MutableDocument;


    class Database : private RefCounted {
    public:
        static bool exists(const char* _cblnonnull name,
                           const char *inDirectory)
        {
            return cbl_databaseExists(name, inDirectory);
        }

        static void copyDB(const char* _cblnonnull fromPath,
                           const char* _cblnonnull toName,
                           const CBLDatabaseConfiguration* config)
        {
            CBLError error;
            check( cbl_copyDB(fromPath, toName, config, &error), error );
        }

        static void deleteDB(const char _cblnonnull *name,
                             const char *inDirectory)
        {
            CBLError error;
            check( cbl_deleteDB(name, inDirectory, &error), error);
        }

        Database(const char *name _cblnonnull,
                 const CBLDatabaseConfiguration& config)
        {
            CBLError error;
            _ref = (CBLRefCounted*) cbl_db_open(name, &config, &error);
            check(_ref != nullptr, error);
        }

        void close() {
            CBLError error;
            check(cbl_db_close(ref(), &error), error);
        }

        void deleteDB() {
            CBLError error;
            check(cbl_db_delete(ref(), &error), error);
        }

        void compact()  {
            CBLError error;
            check(cbl_db_compact(ref(), &error), error);
        }

        const char* name() const                        {return cbl_db_name(ref());}
        const char* path() const                        {return cbl_db_path(ref());}
        uint64_t count() const                          {return cbl_db_count(ref());}
        uint64_t lastSequence() const                   {return cbl_db_lastSequence(ref());}
        const CBLDatabaseConfiguration* config() const  {return cbl_db_config(ref());}

        inline Document getDocument(const char *id _cblnonnull) const;
        inline MutableDocument getMutableDocument(const char *id _cblnonnull) const;

        inline Document saveDocument(MutableDocument &doc,
                                     CBLConcurrencyControl c = kCBLConcurrencyControlFailOnConflict);

        [[nodiscard]] Listener addListener(std::function<void(Database,std::vector<const char*>)>);

        CBL_REFCOUNTED_BOILERPLATE(Database, RefCounted, CBLDatabase)
    };

}
