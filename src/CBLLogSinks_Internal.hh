//
// CBLLogSink_Internal.hh
//
// Copyright © 2024 Couchbase. All rights reserved.
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
#include "CBLLogSinks.h"
#include "Internal.hh"

#include <algorithm>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>

class CBLLogSinks {
public:
    static void init();
    
    static void setConsoleLogSink(CBLConsoleLogSink consoleSink);
    
    static CBLConsoleLogSink consoleLogSink(void);
    
    static void setCustomLogSink(CBLCustomLogSink customSink);
    
    static CBLCustomLogSink customLogSink(void);

    static void setFileLogSink(CBLFileLogSink fileSink); // May throw
    
    static CBLFileLogSink fileLogSink(void);
    
    static void log(CBLLogDomain domain, CBLLogLevel level, const char *msg);
    
    // Temporarily until removing the old logging API
    enum class LogAPIStyle {
        none,
        oldStyle,
        newStyle,
    };
    static void validateAPIUsage(LogAPIStyle usage);
    
    // For testing purpose
    static void reset(void);
    static void logWithC4Log(CBLLogDomain domain, CBLLogLevel level, const char *msg);
    
private:
    static std::atomic<LogAPIStyle> _sAPIStyle;
    
    static std::atomic<CBLLogLevel> _sDomainsLogLevel;
    static std::atomic<CBLLogLevel> _sCallbackLogLevel;
    
    static CBLConsoleLogSink _sConsoleSink;
    static CBLCustomLogSink _sCustomSink;
    static CBLFileLogSink _sFileSink;
    
    static std::shared_mutex _sMutex;
    
    static void _setConsoleLogSink(const CBLConsoleLogSink&);
    static void _setCustomLogSink(const CBLCustomLogSink&);
    static void _setFileLogSink(const CBLFileLogSink&);
    static void updateLogLevels();
    
    static void c4LogCallback(C4LogDomain c4Domain, C4LogLevel c4Level, const char *msg, va_list args);
    static void logToConsoleLogSink(CBLConsoleLogSink&, CBLLogDomain, CBLLogLevel, const char *);
    static void logToCustomLogSink(CBLCustomLogSink&, CBLLogDomain, CBLLogLevel, const char *);
    
    static CBLLogDomain toCBLLogDomain(C4LogDomain);
    static std::string toLogDomainName(CBLLogDomain);
    static std::string toLogLevelName(CBLLogLevel);
};
