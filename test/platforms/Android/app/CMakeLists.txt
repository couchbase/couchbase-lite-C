# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.18.1)

# Declares and names the project.

project("cbl_tests")

set(CBL_DIR ${PROJECT_SOURCE_DIR}/../../../..)
set(TEST_DIR ${CBL_DIR}/test)

# Setup path for find_cmake to find libcblite:
set(LIB_CBLITE_DIR ${PROJECT_SOURCE_DIR}/libs/libcblite)
list(APPEND CMAKE_PREFIX_PATH ${LIB_CBLITE_DIR})

set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
find_package(CouchbaseLite REQUIRED)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

add_definitions(
    -DCBL_TESTS
    -DCATCH_CONFIG_CPP11_STREAM_INSERTABLE_CHECK
    -DNOMINMAX # Get rid of min/max macros that interfere with std::min/std::max
)

include_directories(
    ${LIB_CBLITE_DIR}/include                # libcblite
    ${CBL_DIR}/src                           # For CBLPrivate.h
    ${TEST_DIR}
    ${CBL_DIR}/vendor/couchbase-lite-core/vendor/fleece/API/
    ${CBL_DIR}/vendor/couchbase-lite-core/vendor/fleece/vendor/catch/
    ${CBL_DIR}/vendor/couchbase-lite-core/vendor/fleece/vendor/libb64/
    ${CBL_DIR}/vendor/couchbase-lite-core/vendor/fleece/Fleece/Support/
)

# Test source files:
include("${TEST_DIR}/cmake/test_source_files.cmake")
set_test_source_files(DIR ${TEST_DIR} RESULT TEST_SRC)

set(LIB_SRC
    ${TEST_SRC}
    src/main/cpp/native-lib.cpp
    ${CBL_DIR}/vendor/couchbase-lite-core/vendor/fleece/vendor/catch/catch_amalgamated.cpp
    ${CBL_DIR}/vendor/couchbase-lite-core/vendor/fleece/vendor/catch/CaseListReporter.cc
)

add_library(
    cbl_tests
    SHARED
    ${LIB_SRC}
)

target_link_libraries(
    cbl_tests
    cblite
    "android"
    "log"
)