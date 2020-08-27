cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)

if(NOT DEFINED CMAKE_OSX_SYSROOT)
    # Tells Mac builds to use the current SDK's headers & libs, not what's in the OS.
    set(CMAKE_OSX_SYSROOT macosx)      # this line MUST be above the 1st "project()" line!
endif()

project (CouchbaseLite_C)

### BUILD SETTINGS:

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
option(BUILD_ENTERPRISE "Set whether or not to build enterprise edition" OFF)

if(CMAKE_COMPILER_IS_GNUCC)
    # Suppress an annoying note about GCC 7 ABI changes
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
endif()

if(BUILD_ENTERPRISE)
    add_definitions(-DCOUCHBASE_ENTERPRISE)         # Tells CBL it's an EE build
endif()

add_definitions("-DCMAKE")
include(CheckIncludeFile)
include (CheckFunctionExists)
check_include_file(unistd.h CBL_HAVE_UNISTD_H)
check_include_file(direct.h CBL_HAVE_DIRECT_H)
check_function_exists(vasprintf CBL_HAVE_VASPRINTF)

configure_file(
    "${PROJECT_SOURCE_DIR}/include/cbl/cbl_config.h.in"
    "${PROJECT_BINARY_DIR}/include/cbl/cbl_config.h"
)
if(MSVC)
    include("cmake/platform_win.cmake")
elseif(ANDROID)
    include("cmake/platform_android.cmake")
elseif(APPLE)
    include("cmake/platform_apple.cmake")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    include("cmake/platform_linux_desktop.cmake")
else()
    message(FATAL_ERROR "Unknown platform")
endif()

init_vars()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CBL_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CBL_CXX_FLAGS}")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG>
)

### SUBMODULES:

add_subdirectory(vendor/couchbase-lite-core EXCLUDE_FROM_ALL)

### SOURCE FILES:

include_directories("vendor/couchbase-lite-core/C/include"
                    "vendor/couchbase-lite-core/LiteCore/C"
                    "vendor/couchbase-lite-core/LiteCore/Support"
                    "vendor/couchbase-lite-core/vendor/fleece/API"
                    "vendor/couchbase-lite-core/vendor/fleece/Fleece/Support")

if(!MSVC)
    set_source_files_properties(${C_SRC} PROPERTIES COMPILE_FLAGS -Wno-return-type-c-linkage)
endif()

### BUILDING THE LIBRARY:

set_platform_source_files(RESULT PLATFORM_SRC)
set(
    ALL_SRC_FILES
    src/CBLBase.cc
    src/CBLBlob.cc
    src/CBLDatabase.cc
    src/CBLDocument.cc
    src/CBLLog.cc
    src/CBLQuery.cc
    src/CBLReplicator.cc
    src/ConflictResolver.cc
    src/Listener.cc
    src/Util.cc
    ${PLATFORM_SRC}
)

add_library(CouchbaseLiteCStatic STATIC ${ALL_SRC_FILES})
target_include_directories(CouchbaseLiteCStatic PUBLIC include/cbl)
set_platform_include_directories(RESULT PLATFORM_INCLUDE)
target_include_directories(
    CouchbaseLiteCStatic 
    PRIVATE
    src
    vendor/couchbase-lite-core/C
    ${PROJECT_BINARY_DIR}/include/cbl/
    ${PLATFORM_INCLUDE}
)

target_link_libraries(
    CouchbaseLiteCStatic INTERFACE
    LiteCoreStatic
)

file(WRITE empty.cpp)
add_library(CouchbaseLiteC SHARED empty.cpp)
target_include_directories(CouchbaseLiteC PUBLIC include/cbl)
target_include_directories(
    CouchbaseLiteC
    PRIVATE
    src
    vendor/couchbase-lite-core/C
    ${PROJECT_BINARY_DIR}/include/cbl/
    ${PLATFORM_INCLUDE}
)

install (TARGETS CouchbaseLiteC
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         PUBLIC_HEADER DESTINATION include)

set(CBL_LIBRARIES_PRIVATE  ${WHOLE_LIBRARY_FLAG}
                            CouchbaseLiteCStatic
                            FleeceStatic
                            LiteCoreStatic
                           ${NO_WHOLE_LIBRARY_FLAG}
                            LiteCoreWebSocket)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^armv[67]")
    # C/C++ atomic operations on ARM6/7 emit calls to functions in libatomic
    set(CBL_LIBRARIES_PRIVATE ${CBL_LIBRARIES_PRIVATE} 
                              atomic)
endif()

target_link_libraries(CouchbaseLiteC PRIVATE ${CBL_LIBRARIES_PRIVATE})

set_dylib_properties()

### TESTS:

add_subdirectory(test)
