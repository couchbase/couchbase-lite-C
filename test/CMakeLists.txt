cmake_minimum_required (VERSION 3.1)
cmake_policy(VERSION 3.1)
project (CBL_C_Tests)

set(TOP ${PROJECT_SOURCE_DIR}/../)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()
    
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

# Test source files:
include("cmake/test_source_files.cmake")
set_test_source_files(DIR ${PROJECT_SOURCE_DIR} RESULT TEST_SRC)

add_executable(CBL_C_Tests ${TEST_SRC})

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
elseif(APPLE)
    target_sources(
        CBL_C_Tests PRIVATE
        Platform_Apple.mm
    )
    if(BUILD_ENTERPRISE)
        target_link_libraries(CBL_C_Tests PUBLIC  "-framework CoreFoundation"
                                                  "-framework CoreML"
                                                  "-framework Vision")
    endif()
elseif(UNIX)
    target_link_libraries(CBL_C_Tests PUBLIC dl)
endif()

file(COPY names_100.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/test)
