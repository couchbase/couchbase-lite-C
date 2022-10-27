//
//  CBLCatchTests.m
//  CouchbaseLiteTests-iOS-AppTests
//
//  Created by Pasin Suriyentrakorn on 10/24/22.
//  Copyright © 2022 Couchbase. All rights reserved.
//

#import <XCTest/XCTest.h>

#define CATCH_CONFIG_CONSOLE_WIDTH 120
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "CaseListReporter.hh"

@interface CBLCatchTests : XCTestCase

@end

@implementation CBLCatchTests

- (void)setUp { }

- (void)tearDown { }

- (void)testCatchTests {
    Catch::Session session;
    session.configData().reporterName = "list";

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

    XCTAssertEqual(session.applyCommandLine(argc, argv), 0);
    XCTAssertEqual(session.run(), 0);
}

@end
