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

#if __APPLE__
#include <sys/utsname.h>
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
static string getDeviceModel(const char* fallback) {
    utsname uts;
    if(uname(&uts) != 0) {
        return fallback;
    }
    return uts.machine;
}

#endif
string getAppleVersion();
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

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

using stringstream = std::stringstream;
using alloc_slice = fleece::alloc_slice;

// TEMPLATE - “CouchbaseLite”/<version> “-” <build #> ” (Java; ” <Android API> “;” <device id> “) ” <build type> “, Commit/” (“unofficial@” <hostname> | <git commit>) ” Core/” <core version>
// OUTPUT   - CouchbaseLite/3.1.0-SNAPSHOT (Java; Android 11; Pixel 4a) EE/debug, Commit/unofficial@HQ-Rename0337 Core/3.1.0

static string userAgentHeader(){
        stringstream header;
        string os;
        alloc_slice coreVersion = c4_getVersion();
        alloc_slice coreBuild = c4_getBuildInfo();
#if defined (__APPLE__) && defined (__MACH__)
#if TARGET_IPHONE_SIMULATOR
    os = "iOS Simulator " + getAppleVersion();
#elif TARGET_OS_IPHONE
    os = getDeviceModel("iOS Device") + ' ' + getAppleVersion();
#elif TARGET_OS_MAC
    os = "macOS " + getAppleVersion();
#endif
#elif __ANDROID__
        char rel_ver_str[PROP_VALUE_MAX];
        char sdk_ver_str[PROP_VALUE_MAX];
        __system_property_get("ro.build.version.sdk", sdk_ver_str);
        __system_property_get("ro.build.version.release", rel_ver_str);
        os = "Android " + std::string(rel_ver_str) + " - API " + std::string(sdk_ver_str);
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
                << CBLITE_SOURCE_ID
                << " Core/"
                << coreVersion.asString();

        return header.str();
}
