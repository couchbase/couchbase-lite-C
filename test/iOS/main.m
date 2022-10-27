//
//  main.m
//  CouchbaseLiteTests-iOS-App
//
//  Created by Pasin Suriyentrakorn on 10/24/22.
//  Copyright © 2022 Couchbase. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

int main(int argc, char * argv[]) {
    NSString * appDelegateClassName;
    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
        appDelegateClassName = NSStringFromClass([AppDelegate class]);
    }
    return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
