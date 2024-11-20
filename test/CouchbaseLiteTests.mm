//
//  CouchbaseLiteTests.mm
//  Copyright © 2019 Couchbase. All rights reserved.
//
//  XCTest wrapper for unit tests; for use with Xcode.

#import <XCTest/XCTest.h>

#include "catch.hpp"

@interface CouchbaseLiteTests : XCTestCase
@end

@implementation CouchbaseLiteTests

- (void)testCatchTests {
    NSArray* args = [NSProcessInfo.processInfo arguments];
    NSUInteger nargs = args.count;
    const char* argv[nargs];
    int argc = 0;
    for (NSUInteger i = 0; i < nargs; i++) {
        const char* arg = [args[i] UTF8String];
        if (i > 0 && arg[0] == '-' && isupper(arg[1])) {    // Ignore Cocoa arguments
            ++i;
            continue;
        }
        argv[argc++] = arg;
    }

    Catch::Session session;
    XCTAssertEqual(session.applyCommandLine(argc, argv), 0);
    XCTAssertEqual(session.run(), 0);
}

@end
