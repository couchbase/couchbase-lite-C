//
// CBLuserAgent.hh
//
// Copyright (c) 2022 Couchbase, Inc All rights reserved.
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
#include "CBL_Edition.h"
#include <sstream>

using string = std::string;

#ifdef _MSC_VER
#define NOMINMAX
#include <windows.h>
#pragma comment(lib, "ntdll")

// It's a pain to get the actual header, so just add the function def here
extern "C" NTSYSAPI NTSTATUS NTAPI RtlGetVersion(
    _Out_ PRTL_OSVERSIONINFOW lpVersionInformation
);
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#include <vector>
#include <regex>
#include <optional>
#include <fstream>
#include <sys/utsname.h>

static std::optional<string> tryKey(const char* filename, string&& key) {
    static const std::regex r("(.*)=(.*)");
    std::ifstream fin(filename);
    if(!fin) {
        return {};
    }

    fin.exceptions(std::ios_base::badbit);
    string line;
    std::smatch match;
    while(std::getline(fin, line)) {
        if(std::regex_match(line, match, r)) {
            if(match[1] == key) {
                return match[2];
            }
        }
    }

    return {};
}

static string getDistroInfo() {
    // os-release is apparently the standard these days
    if(auto os = tryKey("/etc/os-release", "PRETTY_NAME")) {
        return *os;
    }

    if(auto os = tryKey("/usr/lib/os-release", "PRETTY_NAME")) {
        return *os;
    }

    // Fall back to the non-standard lsb-release
    if(auto lsb = tryKey("/etc/lsb-release", "DISTRIB_DESCRIPTION")) {
        return *lsb;
    }

    if(auto lsb = tryKey("/etc/lsb-release", "DISTRIB_ID")) {
        return *lsb;
    }

    // Last resort, use uname
    utsname uts;
    if(uname(&uts) != 0) {
        return "Unknown Linux";
    }

    return string(uts.sysname) + ' ' + uts.release;
}
#endif

using stringstream = std::stringstream;
using alloc_slice = fleece::alloc_slice;

static std::string getCCommit(){
    std::string s(CBLITE_SOURCE_ID);
    if (s.size() == 27){
        return s.substr(20);
    }else {
        return "No information";
    }
}

// JAVA TEMPLATE - “CouchbaseLite”/<version> “-” <build #> ” (Java; ” <Android API> “;” <device id> “) ” <build type> “, Commit/” (“unofficial@” <hostname> | <git commit>) ” Core/” <core version>
// JAVA OUTPUT   - CouchbaseLite/3.1.0-SNAPSHOT (Java; Android 11; Pixel 4a) EE/debug, Commit/unofficial@HQ-Rename0337 Core/3.1.0

static string createUserAgentHeader(){
        stringstream header;
        string os;
        std::string cCommit = getCCommit();                                        
        alloc_slice coreVersion = c4_getVersion();
        alloc_slice coreBuild = c4_getBuildInfo();
#if defined (__APPLE__) && defined (__MACH__)
    #if TARGET_IPHONE_SIMULATOR == 1
        os = "Apple iOS Simulator";
    #elif TARGET_OS_IPHONE == 1
        os = "Apple iOS Device";
    #elif TARGET_OS_MAC == 1
        os = "Apple OSX";
    #endif
#elif __ANDROID__
        os = "Android" + std::to_string(__ANDROID_API__);
#elif _MSC_VER
        RTL_OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
        auto result = RtlGetVersion(&version);
        stringstream osStream;
        if (result < 0) {
            os = "Microsoft Windows (Version Fetch Failed)";
        } else {
            osStream << "Microsoft Windows " << version.dwMajorVersion << "." << version.dwMinorVersion << "." << version.dwBuildNumber;
            os = osStream.str();
        }
#elif __linux__
        os = getDistroInfo();
#else
        os = "Unknown OS";
#endif
        header << "CouchbaseLite/"
                << CBLITE_VERSION
                << "-"
                << CBLITE_BUILD_NUMBER
                << " ("
                << os
                << ") "
                << "Commit/"
                << cCommit
                << " ---> Core/"
                << coreVersion.asString();

        return header.str();
}