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
#include "c4Base.h"
#include <cstdlib>
#include "betterassert.hh"


static const C4LogDomain kC4Domains[5] = {
    kC4DefaultLog, kC4DatabaseLog, kC4QueryLog, kC4SyncLog, kC4WebSocketLog
};

static CBLLogCallback sCallback;


CBLLogLevel CBLLog_ConsoleLevelOfDomain(CBLLogDomain domain) CBLAPI {
    precondition((domain <= kCBLLogDomainNetwork));
    return CBLLogLevel(c4log_getLevel(kC4Domains[domain]));
}


CBLLogLevel CBLLog_ConsoleLevel() CBLAPI {
    return CBLLog_ConsoleLevelOfDomain(kCBLLogDomainAll);
}


void CBLLog_SetConsoleLevelOfDomain(CBLLogDomain domain, CBLLogLevel level) CBLAPI {
    precondition((domain <= kCBLLogDomainNetwork));
    precondition((level <= CBLLogNone));
    if (domain == kCBLLogDomainAll) {
        c4log_setCallbackLevel(C4LogLevel(level));
        for (int i = 0; i < 5; ++i)
            c4log_setLevel(kC4Domains[i], C4LogLevel(level));
    } else {
        c4log_setLevel(kC4Domains[domain], C4LogLevel(level));
    }
}


void CBLLog_SetConsoleLevel(CBLLogLevel level) CBLAPI {
    CBLLog_SetConsoleLevelOfDomain(kCBLLogDomainAll, level);
}


bool CBLLog_WillLogToConsole(CBLLogDomain domain, CBLLogLevel level) CBLAPI {
    return (domain <= kCBLLogDomainNetwork) && (level <= CBLLogNone)
        && c4log_willLog(kC4Domains[domain], C4LogLevel(level));
}


void CBLLog_SetCallback(CBLLogCallback callback) CBLAPI {
    auto c4Callback = [](C4LogDomain domain, C4LogLevel level, const char *msg, va_list) noexcept {
        // Map C4LogDomain to CBLLogDomain:
        auto callback = sCallback;
        if (!callback)
            return;
        CBLLogDomain cblDomain = kCBLLogDomainAll;
        for (int d = 0; d < 5; ++d) {
            if (kC4Domains[d] == domain) {
                cblDomain = CBLLogDomain(d);
                break;
            }
        }
        callback(cblDomain, CBLLogLevel(level), msg);
    };

    sCallback = callback;
    c4log_writeToCallback(c4log_callbackLevel(),
                          callback ? c4Callback : nullptr,
                          true);
}


CBLLogCallback CBLLog_Callback() CBLAPI {
    return sCallback;
}


void CBL_Log(CBLLogDomain domain, CBLLogLevel level, const char *format _cbl_nonnull, ...) CBLAPI {
    precondition((domain <= kCBLLogDomainNetwork));
    precondition((level <= CBLLogNone));
    char *message = nullptr;
    va_list args;
    va_start(args, format);
    vasprintf(&message, format, args);
    va_end(args);
    C4LogToAt(kC4Domains[domain], C4LogLevel(level), "%s", message);
    free(message);
}


void CBL_Log_s(CBLLogDomain domain, CBLLogLevel level, FLSlice message) CBLAPI {
    precondition((domain <= kCBLLogDomainNetwork));
    precondition((level <= CBLLogNone));
    c4slog(kC4Domains[domain], C4LogLevel(level), message);
}


const CBLLogFileConfiguration* CBLLog_FileConfig() CBLAPI {
    //TODO: Implement file logging API
    abort();
}

void CBLLog_SetFileConfig(CBLLogFileConfiguration) CBLAPI {
    //TODO: Implement file logging API
    abort();
}
