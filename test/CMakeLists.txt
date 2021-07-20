cmake_minimum_required (VERSION 3.9)
cmake_policy(VERSION 3.9)
project (CBL_C_Tests)

set(TOP ${PROJECT_SOURCE_DIR}/../)

add_definitions(
    -DCBL_TESTS
    -DCATCH_CONFIG_CPP11_STREAM_INSERTABLE_CHECK
    -DNOMINMAX # Get rid of min/max macros that interfere with std::min/std::max
)

include_directories(
    ${TOP}test/
    ${TOP}include/
    ${TOP}/src/        # Just for CBLPrivate.h
    ${TOP}vendor/couchbase-lite-core/vendor/fleece/API/
    ${TOP}vendor/couchbase-lite-core/vendor/fleece/vendor/catch/
    ${TOP}vendor/couchbase-lite-core/vendor/fleece/vendor/libb64/
    ${PROJECT_BINARY_DIR}/../generated_headers/private/cbl/ # Real independent projects won't need this
    ${PROJECT_BINARY_DIR}/../generated_headers/public/cbl/ # or this
)

set(TEST_SRC
    BlobTest_Cpp.cc
    CBLTest.c
    CBLTest.cc
    CBLTestsMain.cpp
    DatabaseTest.cc
    DatabaseTest_Cpp.cc
    QueryTest.cc
    ReplicatorTest.cc
    ReplicatorEETest.cc
    ${TOP}vendor/couchbase-lite-core/vendor/fleece/Fleece/Support/Backtrace.cc
    ${TOP}vendor/couchbase-lite-core/vendor/fleece/Fleece/Support/LibC++Debug.cc
    ${TOP}vendor/couchbase-lite-core/vendor/fleece/Fleece/Support/betterassert.cc
)
add_executable(CBL_C_Tests ${TEST_SRC} )

target_link_libraries(CBL_C_Tests PRIVATE  cblite)

if(MSVC)
    # For internal fleece dependencies:
    target_sources(
        CBL_C_Tests PRIVATE
        ${TOP}vendor/couchbase-lite-core/vendor/fleece/MSVC/asprintf.c
        ${TOP}vendor/couchbase-lite-core/vendor/fleece/MSVC/vasprintf-msvc.c
    )
    target_include_directories(
        CBL_C_Tests PRIVATE
        ${TOP}vendor/couchbase-lite-core/MSVC
        ${TOP}vendor/couchbase-lite-core/vendor/fleece/MSVC
    )

    set(BIN_TOP "${PROJECT_BINARY_DIR}/..")
    set(FilesToCopy ${BIN_TOP}/\$\(Configuration\)/cblite)

    add_custom_command(TARGET CBL_C_Tests POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        -DFilesToCopy="${FilesToCopy}"
        -DDestinationDirectory=${PROJECT_BINARY_DIR}/\$\(Configuration\)
        -P ${TOP}vendor/couchbase-lite-core/MSVC/copy_artifacts.cmake)
elseif(ANDROID)
    target_link_libraries(CBL_C_Tests PUBLIC  "log")
elseif(APPLE AND BUILD_ENTERPRISE)
    target_link_libraries(CBL_C_Tests PUBLIC  "-framework CoreFoundation"
                                          "-framework CoreML"
                                          "-framework Vision")
elseif(UNIX)
    target_link_libraries(CBL_C_Tests PUBLIC dl)
endif()

if(CBLITE_HAS_LTO)
    set_property(TARGET CBL_C_Tests PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

file(COPY names_100.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test)
