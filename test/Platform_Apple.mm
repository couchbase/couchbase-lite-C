//
// Platform_Apple.mm
//
// Copyright © 2022 Couchbase. All rights reserved.
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

#ifdef __APPLE__

#include "Platform_Apple.hh"
#include <Foundation/Foundation.h>

std::string GetTempDirectory(std::string subdir) {
    NSString* tempDir = NSTemporaryDirectory();
    if (!subdir.empty()) {
        NSString* sub = [NSString stringWithCString: subdir.c_str() encoding: NSUTF8StringEncoding];
        tempDir = [tempDir stringByAppendingPathComponent: sub];
    }
    return std::string([tempDir UTF8String]);
}

#endif
