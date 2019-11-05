cmake_minimum_required (VERSION 2.6)
project (CBL_C_Tests)

set(TOP ${PROJECT_SOURCE_DIR}/../)

add_definitions(-DCBL_TESTS)

include_directories(${TOP}test/
                    ${TOP}vendor/couchbase-lite-core/vendor/fleece/API/
                    ${TOP}vendor/couchbase-lite-core/vendor/fleece/vendor/catch/
                    ${TOP}include/
                    ${TOP}include/cbl++/
                    ${TOP}/src/        # Just for CBLPrivate.h
                    ${PROJECT_BINARY_DIR}/../include/cbl/
                )

set(TEST_SRC
    BlobTest_Cpp.cc
    CBLTest.c
    CBLTest.cc
    CBLTestsMain.cpp
    DatabaseTest.cc
    DatabaseTest_Cpp.cc
    QueryTest.cc
    ReplicatorTest.cc)

add_executable(CBL_C_Tests ${TEST_SRC} )

target_link_libraries(CBL_C_Tests PRIVATE  CouchbaseLiteC FleeceStatic)

if(MSVC)
    set(BIN_TOP "${PROJECT_BINARY_DIR}/../..")
    set(FilesToCopy ${BIN_TOP}/\$\(Configuration\)/LiteCore
                    ${BIN_TOP}/\$\(Configuration\)/LiteCoreREST)
    add_custom_command(TARGET CBL_C_Tests POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        -DFilesToCopy="${FilesToCopy}"
        -DDestinationDirectory=${PROJECT_BINARY_DIR}/\$\(Configuration\)
        -P ${TOP}MSVC/copy_artifacts.cmake)
elseif(ANDROID)
    target_link_libraries(CBL_C_Tests PUBLIC  "log")
elseif(APPLE AND BUILD_ENTERPRISE)
    target_link_libraries(CBL_C_Tests PUBLIC  "-framework CoreFoundation"
                                          "-framework CoreML"
                                          "-framework Vision")
elseif(UNIX)
    target_link_libraries(CBL_C_Tests PUBLIC  "pthread" "${LIBCXX_LIB}" "${LIBCXXABI_LIB}" dl)
endif()
