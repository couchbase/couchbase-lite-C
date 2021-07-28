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
#include "CBLPrivate.h"
#include "c4Base.hh"
#include "Internal.hh"
#include "fleece/slice.hh"
#include "betterassert.hh"
#include "LogDecoder.hh"
#include "ParseDate.hh"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <mutex>

using namespace std;
using namespace fleece;
using namespace litecore;

static const C4LogDomain kC4Domains[] = { kC4DatabaseLog, kC4QueryLog, kC4SyncLog, kC4WebSocketLog };

static const char* const kLogLevelNames[] = {"debug", "verbose", "info", "warning", "error"};

static const size_t kDefaultLogFileConfigMaxSize = 500 * 1024;
static const int32_t kDefaultLogFileConfigMaxRotateCount = 1;

static atomic<CBLLogLevel> sConsoleLogLevel = kCBLLogWarning;

static atomic<CBLLogCallback> sCustomCallback = nullptr;
static atomic<CBLLogLevel> sCustomLogLevel = kCBLLogWarning;

static CBLLogFileConfiguration sLogFileConfig;
static alloc_slice sLogFileDir;

static void c4LogCallback(C4LogDomain domain, C4LogLevel level, const char *fmt, va_list args);

static void init();

static C4LogLevel effectiveC4CallbackLogLevel();

static void updateC4CallbackLogLevel();

static CBLLogDomain getCBLLogDomain(C4LogDomain domain);

// Note: Cannot use static initializing here as the order of initializing static C4LogDomain
// constants such as kC4DatabaseLog cannot be guaranteed to be done prior.
static once_flag initFlag;
static void init() {
    call_once(initFlag, [](){
        // Initialize log level of each domain to debug (lowest level):
        for (int i = 0; i < sizeof(kC4Domains)/sizeof(kC4Domains[0]); ++i) {
            C4LogDomain domain = kC4Domains[i];
            c4log_setLevel(domain, kC4LogDebug);
        }
        
        // Register log callback:
        c4log_writeToCallback(effectiveC4CallbackLogLevel(), &c4LogCallback, true /*preformatted*/);
    });
}


CBLLogLevel CBLLog_ConsoleLevel() CBLAPI {
    return sConsoleLogLevel;
}


void CBLLog_SetConsoleLevel(CBLLogLevel level) CBLAPI {
    init();
    if (sConsoleLogLevel.exchange(level) != level)
        updateC4CallbackLogLevel();
}


CBLLogLevel CBLLog_CallbackLevel() CBLAPI {
    return sCustomLogLevel;
}


void CBLLog_SetCallbackLevel(CBLLogLevel level) CBLAPI {
    init();
    if (sCustomLogLevel.exchange(level) != level)
        updateC4CallbackLogLevel();
}


CBLLogCallback CBLLog_Callback() CBLAPI {
    return sCustomCallback;
}


void CBLLog_SetCallback(CBLLogCallback callback) CBLAPI {
    init();
    if (sCustomCallback.exchange(callback) != callback)
        updateC4CallbackLogLevel();
}


static C4LogLevel effectiveC4CallbackLogLevel() {
    CBLLogLevel customLogLevel = sCustomCallback != nullptr ? sCustomLogLevel.load() : kCBLLogNone;
    return C4LogLevel(std::min(sConsoleLogLevel.load(), customLogLevel));
}


static void updateC4CallbackLogLevel() {
    C4LogLevel level = effectiveC4CallbackLogLevel();
    if (c4log_callbackLevel() != level)
        c4log_setCallbackLevel(level);
}


static void c4LogCallback(C4LogDomain domain, C4LogLevel level, const char *msg, va_list args) {
    CBLLogLevel msgLevel = CBLLogLevel(level);
    
    // Log to console:
    CBLLogLevel consoleLogLevel = sConsoleLogLevel;
    if (msgLevel >= consoleLogLevel) {
        auto domainName = c4log_getDomainName(domain);
        auto levelName = kLogLevelNames[(int)level];
        
        ostream& os = msgLevel < kCBLLogWarning ? cout : cerr;
        LogDecoder::writeTimestamp(LogDecoder::now(), os);
        LogDecoder::writeHeader(levelName, domainName, os);
        os << msg << '\n';
    }
    
    // Log to custom callback if available:
    CBLLogCallback callback = sCustomCallback;
    if (!callback)
        return;
    
    CBLLogLevel customLogLevel = sCustomLogLevel;
    if (msgLevel >= customLogLevel) {
        // msg is preformatted
        callback(getCBLLogDomain(domain), msgLevel, slice(msg));
    }
}


static CBLLogDomain getCBLLogDomain(C4LogDomain domain) {
    CBLLogDomain cblDomain = kCBLLogDomainDatabase;
    for (int i = 0; i < sizeof(kC4Domains)/sizeof(kC4Domains[0]); i++) {
        if (kC4Domains[i] == domain) {
            cblDomain = CBLLogDomain(i);
            break;
        }
    }
    return cblDomain;
}


void CBL_Log(CBLLogDomain domain, CBLLogLevel level, const char *format, ...) CBLAPI {
    precondition((domain <= kCBLLogDomainNetwork));
    precondition((level <= kCBLLogNone));
    char *message = nullptr;
    va_list args;
    va_start(args, format);
    vasprintf(&message, format, args);
    va_end(args);
    C4LogToAt(kC4Domains[domain], C4LogLevel(level), "%s", message);
    free(message);
}


void CBL_LogMessage(CBLLogDomain domain, CBLLogLevel level, FLString message) CBLAPI {
    precondition((domain <= kCBLLogDomainNetwork));
    precondition((level <= kCBLLogNone));
    if (message.buf == nullptr)
        return;
    CBL_Log(domain, level, "%.*s", (int) message.size, (char *) message.buf);
}


const CBLLogFileConfiguration* CBLLog_FileConfig() CBLAPI {
    if (sLogFileConfig.directory.buf)
        return &sLogFileConfig;
    else
        return nullptr;
}


bool CBLLog_SetFileConfig(CBLLogFileConfiguration config, CBLError *outError) CBLAPI {
    init();
    
    sLogFileDir = config.directory;     // copy string to the heap
    config.directory = sLogFileDir;     // and put the heap copy in the struct
    sLogFileConfig = config;

    alloc_slice buildInfo = c4_getBuildInfo();
    string header = "Generated by Couchbase Lite for C / " + string(buildInfo);

    C4LogFileOptions c4opt = {};
    c4opt.log_level         = C4LogLevel(config.level);
    c4opt.base_path         = config.directory;
    c4opt.max_size_bytes    = config.maxSize > 0 ? config.maxSize : kDefaultLogFileConfigMaxSize;
    c4opt.max_rotate_count  = config.maxRotateCount > 0 ? config.maxRotateCount : kDefaultLogFileConfigMaxRotateCount;
    c4opt.use_plaintext     = config.usePlaintext;
    c4opt.header            = slice(header);
    
    return c4log_writeToBinaryFile(c4opt, internal(outError));
}


extern "C" CBL_CORE_API std::atomic_int gC4ExpectExceptions;

void CBLLog_BeginExpectingExceptions() CBLAPI {
    ++gC4ExpectExceptions;
    c4log_warnOnErrors(false);
}

void CBLLog_EndExpectingExceptions() CBLAPI {
    if (--gC4ExpectExceptions == 0)
        c4log_warnOnErrors(true);
}
