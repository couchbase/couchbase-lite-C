//
// CBLCompat.h
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

#if defined(__clang__)
    #define CBL_START_WARNINGS_SUPPRESSION _Pragma( "clang diagnostic push" )
    #define CBL_STOP_WARNINGS_SUPPRESSION _Pragma( "clang diagnostic pop" )
    #define CBL_IGNORE_DEPRECATED_API _Pragma( "clang diagnostic ignored \"-Wdeprecated-declarations\"" )
#elif defined(__GNUC__)
    #define CBL_START_WARNINGS_SUPPRESSION _Pragma( "GCC diagnostic push" )
    #define CBL_STOP_WARNINGS_SUPPRESSION _Pragma( "GCC diagnostic pop" )
    #define CBL_IGNORE_DEPRECATED_API _Pragma( "GCC diagnostic ignored \"-Wdeprecated-declarations\"" )
#elif defined(_MSC_VER)
    #define CBL_START_WARNINGS_SUPPRESSION __pragma( warning(push) )
    #define CBL_STOP_WARNINGS_SUPPRESSION __pragma( warning(pop) )
    #define CBL_IGNORE_DEPRECATED_API
#else
    #define CBL_START_WARNINGS_SUPPRESSION
    #define CBL_STOP_WARNINGS_SUPPRESSION
    #define CBL_IGNORE_DEPRECATED_API
#endif
