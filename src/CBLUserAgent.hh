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
using stringstream = std::stringstream;
using alloc_slice = fleece::alloc_slice;

// JAVA TEMPLATE - “CouchbaseLite”/<version> “-” <build #> ” (Java; ” <Android API> “;” <device id> “) ” <build type> “, Commit/” (“unofficial@” <hostname> | <git commit>) ” Core/” <core version>
// JAVA OUTPUT   - CouchbaseLite/3.1.0-SNAPSHOT (Java; Android 11; Pixel 4a) EE/debug, Commit/unofficial@HQ-Rename0337 Core/3.1.0

    static string createUserAgentHeader(){
            stringstream header = {};
            string os;
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
        os = "Android" + string(__ANDROID_API__);
#elif  _WIN64
        os = "Microsoft Windows (64-bit)";
#elif _WIN32
        os = "Microsoft Windows (32-bit)";
#elif __linux__
        os = "Linux";
#else 
        os = "Unknown OS";
#endif
        header << "CouchbaseLite/"
               << CBLITE_VERSION
               << "-"
               << CBLITE_BUILD_NUMBER
               << " ("
               << os
               << ") Core/"
               << coreVersion.asString();
        
        return header.str();
}
