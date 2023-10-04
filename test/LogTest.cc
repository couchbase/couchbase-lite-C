//
// LogTest.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "CBLTest.hh"
#include "cbl/CouchbaseLite.h"
#include "fleece/Fleece.hh"
#include <array>
#include <dirent.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

using namespace std;
using namespace fleece;

#ifdef _MSC_VER
    static const char  kSeparatorChar = '\\';
    static const char  kBackupSeparatorChar = '/';
    static const char* kCurrentDir = ".\\";
#else
    static const char  kSeparatorChar = '/';
    static const char  kBackupSeparatorChar = '\\';
    static const char* kCurrentDir = "./";
#endif

static const array<CBLLogLevel, 6> kLogLevels = {{
    kCBLLogDebug, kCBLLogVerbose, kCBLLogInfo, kCBLLogWarning, kCBLLogError, kCBLLogNone
}};

static const array<string, 6> kLogLevelNames = {{
    "Debug", "Verbose", "Info", "Warning", "Error", "None"
}};

static const array<string, 5> kLogFileNamePrefixes = {{
    "cbl_debug_", "cbl_verbose_", "cbl_info_", "cbl_warning_", "cbl_error_"
}};

// For rotating test log directory:
static unsigned sLogDirCount = 0;

class LogTest : public CBLTest {
public:
    string logDir;
    CBLLogLevel backupConsoleLogLevel;
    
    LogTest() {
        backupConsoleLogLevel = CBLLog_ConsoleLevel();
    }

    ~LogTest() {
        // Restore from backup:
        CBLLog_SetConsoleLevel(backupConsoleLogLevel);
        
        // Disable file logging if there is one set up:
        const CBLLogFileConfiguration* oldConfig = CBLLog_FileConfig();
        if (oldConfig != nullptr) {
            CBLLogFileConfiguration config = *oldConfig;
            config.level = kCBLLogNone;
            REQUIRE(CBLLog_SetFileConfig(config, nullptr));
        }
        
        // Reset log callback:
        CBLLog_SetCallback(nullptr);
        CBLLog_SetCallbackLevel(kCBLLogNone);
    }
    
    void prepareLogDir() {
        // Base:
        string dir = string(CBLTest::databaseDir()) + kSeparatorChar + "CBLLogTest";
        createDir(dir);
        
        // Log dir:
        dir += kSeparatorChar + to_string(++sLogDirCount);
        createDir(dir);
        
        logDir = dir;
        deleteAllLogFiles();
    }
    
    void deleteAllLogFiles() {
        auto paths = getAllLogFilePaths();
        for (string path : paths) {
        #ifndef WIN32
            int result = unlink(path.c_str());
        #else
            int result = _unlink(path.c_str());
        #endif
            if (result != 0)
                FAIL("Can't delete file at " << path <<": " << result);
        }
    }
    
    vector<string> getAllLogFilePaths() {
        vector<string> logFiles;
        
        auto dir = opendir(logDir.c_str());
        if (!dir)
            FAIL("Can't open log directory at "<< logDir << ": errno " << errno);
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            string filename = string(entry->d_name);
        #ifndef WIN32
            string path = logDir + kSeparatorChar + filename;
            struct stat statEntry;
            stat(path.c_str(), &statEntry);
        #else
            string path = logDir + kSeparatorChar + filename;
            struct _stat statEntry;
            _stat(path.c_str(), &statEntry);
        #endif
            
            if (S_ISDIR(statEntry.st_mode))
                continue;
            
            auto split = splitExtension(filename);
            if (split.second != ".cbllog")
                continue;
            
            logFiles.push_back(path);
        }
        
        return logFiles;
    }
    
    vector<string> readLogFile(CBLLogLevel level) {
        string filePath;
        auto logFilePaths = getAllLogFilePaths();
        for (string path : logFilePaths) {
            auto split = splitPath(path);
            auto fileName = split.second;
            if (fileName.find(kLogFileNamePrefixes[level]) == 0) {
                // Make sure there is only one
                CHECK(filePath.empty());
                filePath = path;
            }
        }
        REQUIRE(!filePath.empty());
        
        vector<string> lines;
        ReadFileByLines(filePath, [&](FLSlice line) {
            lines.push_back(string(line));
            return true;
        });
        return lines;
    }
    
    void writeLogs() {
        CBL_Log(kCBLLogDomainDatabase, kCBLLogDebug, "%s ...", kLogLevelNames[kCBLLogDebug].c_str());
        CBL_Log(kCBLLogDomainDatabase, kCBLLogVerbose, "%s ...", kLogLevelNames[kCBLLogVerbose].c_str());
        CBL_Log(kCBLLogDomainDatabase, kCBLLogInfo, "%s ...", kLogLevelNames[kCBLLogInfo].c_str());
        CBL_Log(kCBLLogDomainDatabase, kCBLLogWarning, "%s ...", kLogLevelNames[kCBLLogWarning].c_str());
        CBL_Log(kCBLLogDomainDatabase, kCBLLogError, "%s ...", kLogLevelNames[kCBLLogError].c_str());
    }
    
    // File Utils:
    
    void createDir(string dir) {
    #ifndef WIN32
        if (mkdir(dir.c_str(), 0744) != 0 && errno != EEXIST)
            FAIL("Can't create temp directory: errno " << errno);
    #else
        if (_mkdir(dir.c_str()) != 0 && errno != EEXIST)
            FAIL("Can't create temp directory: errno " << errno);
    #endif
    }
    
    pair<string,string> splitExtension(const string &file) {
        auto dot = file.rfind('.');
        auto lastSlash = file.rfind(kSeparatorChar);
        if (dot == string::npos || (lastSlash != string::npos && dot < lastSlash))
            return {file, ""};
        else
            return {file.substr(0, dot), file.substr(dot)};
    }
    
    pair<string,string> splitPath(const string &path) {
        string dirname, basename;
        auto slash = path.rfind(kSeparatorChar);
        auto backupSlash = path.rfind(kBackupSeparatorChar);
        if (slash == string::npos && backupSlash == string::npos) {
            return{ kCurrentDir, string(path) };
        }
        
        if (slash == string::npos) {
            slash = backupSlash;
        }
        else if (backupSlash != string::npos) {
            slash = std::max(slash, backupSlash);
        }

        return {string(path.substr(0, slash+1)), string(path.substr(slash+1))};
    }
};


TEST_CASE_METHOD(LogTest, "Console Logging : Set Log Level", "[Log]") {
    for (CBLLogLevel level : kLogLevels) {
        CBLLog_SetConsoleLevel(level);
        CHECK(CBLLog_ConsoleLevel() == level);
    }
}


TEST_CASE_METHOD(LogTest, "File Logging : Config", "[Log][FileLog]") {
    prepareLogDir();
    
    CBLLogFileConfiguration config = {};
    config.directory = slice(logDir);
    config.level = kCBLLogVerbose;
    config.maxRotateCount = 5;
    config.maxSize = 10;
    config.usePlaintext = true;
    
    CBLError error;
    CHECK(CBLLog_SetFileConfig(config, &error));
    
    auto config2 = CBLLog_FileConfig();
    CHECK(config2->level == config.level);
    CHECK(config2->directory == config.directory);
    CHECK(config2->maxRotateCount == config.maxRotateCount);
    CHECK(config2->maxSize == config.maxSize);
    CHECK(config2->usePlaintext == config.usePlaintext);
}


TEST_CASE_METHOD(LogTest, "File Logging : Set Log Level", "[Log][FileLog]") {
    prepareLogDir();
    
    CBLLogFileConfiguration config = {};
    config.directory = slice(logDir);
    config.usePlaintext = true;
    
    // Set setting different log levels:
    CBLError error;
    for (CBLLogLevel level : kLogLevels) {
        // Set log level:
        config.level = level;
        REQUIRE(CBLLog_SetFileConfig(config, &error));
        
        // Write messages on each log level:
        writeLogs();
    }
    
    // Verify:
    int lineCount = 2; // Header (2 lines):
    for (CBLLogLevel level : kLogLevels) {
        if (level == kCBLLogNone)
            continue;
        
        vector<string> lines = readLogFile(level);
        REQUIRE(lines.size() == ++lineCount);
    }
}


TEST_CASE_METHOD(LogTest, "Custom Logging", "[Log][CustomLog]") {
    // Set log callback:
    static vector<CBLLogLevel>recs;
    CBLLog_SetCallback([](CBLLogDomain domain, CBLLogLevel level, FLString msg) {
        CHECK(level >= CBLLog_CallbackLevel());
        CHECK(string(msg).find(kLogLevelNames[level]) == 0);
        recs.push_back(level);
    });
    
    // Set setting different log levels:
    for (CBLLogLevel callbackLevel : kLogLevels) {
        // Set log level:
        CBLLog_SetCallbackLevel(callbackLevel);
        CHECK(CBLLog_CallbackLevel() == callbackLevel);
        
        // Write messages on each log level:
        recs.clear();
        writeLogs();
        
        // Verify:
        for (CBLLogLevel level : kLogLevels) {
            if (level == kCBLLogNone)
                continue;
            if (level >= callbackLevel)
                CHECK(find(recs.begin(), recs.end(), level) != recs.end());
            else
                CHECK(find(recs.begin(), recs.end(), level) == recs.end());
        }
    }
    
    // Reset log callback:
    CBLLog_SetCallbackLevel(kCBLLogDebug);
    CBLLog_SetCallback(nullptr);
    REQUIRE(CBLLog_Callback() == nullptr);
    recs.clear();
    writeLogs();
    CHECK(recs.empty());
    CBLLog_SetCallbackLevel(kCBLLogNone);
}


TEST_CASE_METHOD(LogTest, "Log Message", "[Log]") {
    static vector<string>recs;
    CBLLog_SetCallback([](CBLLogDomain domain, CBLLogLevel level, FLString msg) {
        recs.push_back(string(msg));
    });
    CBLLog_SetCallbackLevel(kCBLLogDebug);
    
    // Use CBL_Log:
    CBL_Log(kCBLLogDomainDatabase, kCBLLogInfo, "foo %s", "bar");
    
    // Use CBL_LogMessage:
    CBL_LogMessage(kCBLLogDomainDatabase, kCBLLogInfo, "hello world"_sl);
    
    REQUIRE(recs.size() == 2);
    CHECK(recs[0] == "foo bar");
    CHECK(recs[1] == "hello world");
}
