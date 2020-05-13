//
// CBLDatabase+ObjC.mm
//
// Copyright © 2020 Couchbase. All rights reserved.
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

#import <Foundation/Foundation.h>
#include "CBLDatabase_Internal.hh"


std::string CBLDatabase::defaultDirectory() {
    @autoreleasepool {
        // Start in the Application Support directory,
        // except on tvOS which only allows apps to store data in the Caches directory
        NSSearchPathDirectory dirID = NSApplicationSupportDirectory;
#if TARGET_OS_TV
        dirID = NSCachesDirectory; // Apple TV only allows apps to store data in the Caches directory
#endif
        NSArray* paths = NSSearchPathForDirectoriesInDomains(dirID, NSUserDomainMask, YES);
        NSString* path = paths[0];

#if !TARGET_OS_IPHONE
        // On macOS, append the application's bundle ID to get a per-app directory:
        NSString* bundleID = [[NSBundle mainBundle] bundleIdentifier];
        if (!bundleID) {
            // For non-apps with no bundle ID, just default to the current directory:
            return NSFileManager.defaultManager.currentDirectoryPath.fileSystemRepresentation;
        }
        path = [path stringByAppendingPathComponent: bundleID];
#endif
        // Append a "CouchbaseLite" component to the path:
        return [path stringByAppendingPathComponent: @"CouchbaseLite"].fileSystemRepresentation;
    }
}
