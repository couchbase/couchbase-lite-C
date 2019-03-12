//
// CBLLog.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "CBLLog.h"
#include <cstdlib>

CBLLogLevel cbl_log_consoleLevel()
{
    abort();
}

void cbl_log_setConsoleLevel(CBLLogLevel)
{
    abort();
}

const CBLLogFileConfiguration* cbl_log_fileConfig()
{
    abort();
}

void cbl_log_setFileConfig(CBLLogFileConfiguration)
{
    abort();
}

CBLLogCallback cbl_log_callback()
{
    abort();
}

void cbl_log_setCallback(CBLLogCallback)
{
    abort();
}