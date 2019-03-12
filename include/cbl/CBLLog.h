//
// CBLLog.h
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

#pragma once
#include "CBLBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name  Public API
    @{ */

/** An object containing properties for file logging configuration 
    @warn \ref usePlaintext is not recommended for production */
typedef struct {
    const char* directory;          ///< The directory to write logs to (UTF-8 encoded)
    const uint32_t maxRotateCount;  ///< The maximum number of *rotated* logs to keep (i.e. the total number of logs will be one more)
    const size_t maxSize;           ///< The max size to write to a log file before rotating (best-effort)
    const bool usePlaintext;        ///< Whether or not to log in plaintext
} CBLLogFileConfiguration;

/** A callback function for handling log messages
    @param  level The level of the message being received
    @param  domain The domain of the message being received
    @param  message The message being received (UTF-8 encoded) */
typedef void(*CBLLogCallback)(CBLLogLevel level, CBLLogDomain domain, const char* message);

/** Gets the current log level for debug console logging */
CBLLogLevel cbl_log_consoleLevel();

/** Sets the debug console log level */
void cbl_log_setConsoleLevel(CBLLogLevel);

/** Gets the current file logging config */
const CBLLogFileConfiguration* cbl_log_fileConfig();

/** Sets the file logging configuration */
void cbl_log_setFileConfig(CBLLogFileConfiguration);

/** Gets the current log callback */
CBLLogCallback cbl_log_callback();

/** Sets the callback for receiving log messages */
void cbl_log_setCallback(CBLLogCallback);

/** @} */

#ifdef __cplusplus
}
#endif