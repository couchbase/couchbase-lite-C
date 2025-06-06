cmake_minimum_required (VERSION 3.5)
cmake_policy(VERSION 3.5)

if(NOT DEFINED CMAKE_OSX_SYSROOT)
    # Tells Mac builds to use the current SDK's headers & libs, not what's in the OS.
    set(CMAKE_OSX_SYSROOT macosx)      # this line MUST be above the 1st "project()" line!
endif()

set(CMAKE_OSX_ARCHITECTURES x86_64 arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.14)

if(DEFINED ENV{VERSION})
    message(VERBOSE "Using VERSION:$ENV{VERSION} from environment variable")
    set(CBL_VERSION_STRING $ENV{VERSION})
else()
    message(WARNING "No VERSION set, defaulting to 0.0.0")
    set(CBL_VERSION_STRING "0.0.0")
endif()

project (
    CouchbaseLite_C
    VERSION ${CBL_VERSION_STRING}
)

set(CBL_LIB_VERSION ${CouchbaseLite_C_VERSION})
set(CBL_API_VERSION ${CouchbaseLite_C_VERSION_MAJOR})

### BUILD SETTINGS:

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
option(BUILD_ENTERPRISE "Set whether or not to build enterprise edition" OFF)
option(CODE_COVERAGE_ENABLED "Set whether or not code coverage report should be generated" OFF)
option(STRIP_SYMBOLS "Set whether or not the private symbols will be stripped" OFF)

if(CODE_COVERAGE_ENABLED)
   message("Code coverage enabled, forcing sanitizers off")
   if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
   elseif(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
     message(WARN " Code coverage not supported for non-debug builds")
   else()
     message(WARN " Code coverage only supported on Clang")
   endif()
endif()

if(BUILD_ENTERPRISE)
    message("Building Enterprise Edition")
    set(COUCHBASE_ENTERPRISE 1)                     # For generating CBL_Edition.h
    add_definitions(
        -DCOUCHBASE_ENTERPRISE                       # For building source files that don't include CBL_Edition.h
    )
else()
    message("Building Community Edition")
endif()

include(cmake/generate_edition.cmake)
generate_edition()

add_definitions("-DCMAKE")
include(CheckIncludeFile)
include(CheckFunctionExists)
include(GNUInstallDirs)
check_include_file(unistd.h CBL_HAVE_UNISTD_H)
check_include_file(direct.h CBL_HAVE_DIRECT_H)
check_function_exists(vasprintf CBL_HAVE_VASPRINTF)

configure_file(
    "${PROJECT_SOURCE_DIR}/include/cbl/cbl_config.h.in"
    "${PROJECT_BINARY_DIR}/generated_headers/private/cbl/cbl_config.h"
)

if(MSVC)
    # CBL-6932 : Workaround to avoid crash when locking mutex on VC++ Runtime version < 14.4.
    # Add this before adding any subdirectories so that the added definition is propagated.
    add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
    
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

### BUILDING THE LIBRARY:

set_platform_source_files(RESULT PLATFORM_SRC)
set(
    ALL_SRC_FILES
    src/CBLBase_CAPI.cc
    src/CBLBlob_CAPI.cc
    src/CBLCollection.cc
    src/CBLCollection_CAPI.cc
    src/CBLDatabase.cc
    src/CBLDatabase_CAPI.cc
    src/CBLDefaults_CAPI.cc
    src/CBLDocument.cc
    src/CBLDocument_CAPI.cc
    src/CBLEncryptable_CAPI.cc
    src/CBLLog.cc
    src/CBLLogSinks.cc
    src/CBLLogSinks_CAPI.cc
    src/CBLPrediction.cc
    src/CBLPrediction_CAPI.cc
    src/CBLQuery.cc
    src/CBLQuery_CAPI.cc
    src/CBLQueryIndex.cc
    src/CBLQueryIndex_CAPI.cc
    src/CBLReplicator_CAPI.cc
    src/CBLScope.cc
    src/CBLScope_CAPI.cc
    src/CBLTLSIdentity_CAPI.cc
    src/CBLURLEndpointListener_stub.cc
    src/CBLURLEndpointListener_CAPI.cc
    src/CBLVectorIndexConfig_CAPI.cc
    src/ConflictResolver.cc
    src/ContextManager.cc
    src/Internal.cc
    src/Listener.cc
    ${PLATFORM_SRC}
)

add_library(cblite-static STATIC ${ALL_SRC_FILES})
target_include_directories(
    cblite-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated_headers/public/cbl/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/couchbase-lite-core/vendor/fleece/API/>
    $<INSTALL_INTERFACE:include/>
)
set_platform_include_directories(RESULT PLATFORM_INCLUDE)
target_include_directories(
    cblite-static
    PRIVATE
    # Project includes
    include/cbl
    src

    # Generated headers
    ${PROJECT_BINARY_DIR}/generated_headers/private/cbl

    # Platform-specific includes
    ${PLATFORM_INCLUDE}

    # Enterprise-only includes
    $<$<BOOL:${BUILD_ENTERPRISE}>:${CMAKE_CURRENT_SOURCE_DIR}/../couchbase-lite-c-ee/src>

    # LiteCore and Fleece
    vendor/couchbase-lite-core/C
    vendor/couchbase-lite-core/C/include
    vendor/couchbase-lite-core/C/Cpp_include
    vendor/couchbase-lite-core/LiteCore/Logging
    vendor/couchbase-lite-core/LiteCore/Support
    vendor/couchbase-lite-core/vendor/fleece/API
    vendor/couchbase-lite-core/vendor/fleece/Fleece/Support
)

file(WRITE empty.cpp)
add_library(cblite SHARED empty.cpp)
target_include_directories(
    cblite PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated_headers/public/cbl/>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/couchbase-lite-core/vendor/fleece/API/>
    $<INSTALL_INTERFACE:include/>
)

set(CBL_LIBRARIES_PRIVATE
    ${WHOLE_LIBRARY_FLAG}
    cblite-static
    ${NO_WHOLE_LIBRARY_FLAG}
    LiteCoreObjects
    LiteCoreREST_Objects
    LiteCoreWebSocket
    FleeceObjects
    BLIPObjects
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^armv[67]")
    # C/C++ atomic operations on ARM6/7 emit calls to functions in libatomic
    set(CBL_LIBRARIES_PRIVATE ${CBL_LIBRARIES_PRIVATE}
                              atomic)
endif()

target_link_libraries(cblite PRIVATE ${CBL_LIBRARIES_PRIVATE})

set_dylib_properties()
set_target_properties(cblite PROPERTIES VERSION "${CBL_LIB_VERSION}" SOVERSION "${CBL_API_VERSION}")

## Installation

if(ANDROID)
    set(CMAKE_INSTALL_LIBDIR "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_INSTALL_LIBDIR "lib/${CMAKE_C_LIBRARY_ARCHITECTURE}")
else()
    set(CMAKE_INSTALL_LIBDIR "lib") # No lib64, stop!
endif()

install(
    TARGETS cblite
    EXPORT CouchbaseLiteTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

## CBL Headers
file(GLOB C_HEADERS ${PROJECT_SOURCE_DIR}/include/cbl/*.h)
file(GLOB CPP_HEADERS ${PROJECT_SOURCE_DIR}/include/cbl++/*.hh)

## Fleece Headers
set(FLEECE_HEADER_DIR "${PROJECT_SOURCE_DIR}/vendor/couchbase-lite-core/vendor/fleece/API/fleece")
file(GLOB FLEECE_HEADERS ${FLEECE_HEADER_DIR}/*.h ${FLEECE_HEADER_DIR}/*.hh)

## Exclude Fleece Expert Headers
# file(GLOB FLEECE_EXPERTS ${FLEECE_HEADER_DIR}/*Expert.h*)
# list(REMOVE_ITEM FLEECE_HEADERS ${FLEECE_EXPERTS})

install(EXPORT CouchbaseLiteTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CouchbaseLite)
install(FILES ${C_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbl)
install(FILES ${CPP_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbl++)
install(FILES ${PROJECT_BINARY_DIR}/generated_headers/public/cbl/CBL_Edition.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbl)
install(FILES ${FLEECE_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fleece)

include(CMakePackageConfigHelpers)
set(TARGETS_EXPORT_NAME "CouchbaseLiteTargets")
configure_package_config_file(
  cmake/CouchbaseLiteConfig.cmake.in
  ${CMAKE_BINARY_DIR}/CouchbaseLiteConfig.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/CouchbaseLite"
)
write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/CouchbaseLiteConfigVersion.cmake
    VERSION ${CBL_LIBRARY_VERSION} COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_BINARY_DIR}/CouchbaseLiteConfig.cmake
              ${CMAKE_BINARY_DIR}/CouchbaseLiteConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CouchbaseLite)

## Generate dSYM and strip private symbols (Post Build when STRIP_SYMBOLS is enabled)

if(STRIP_SYMBOLS)
    message("Stripping symbols enabled")
    if(APPLE)
        set(STRIP_SRC_FILE $<TARGET_FILE:cblite>)
        set(SYM_OUT_FILE $<TARGET_FILE:cblite>.dSYM)
        add_custom_command(TARGET cblite POST_BUILD VERBATIM
            COMMAND dsymutil ${STRIP_SRC_FILE} -o ${SYM_OUT_FILE}
            COMMAND strip -x ${STRIP_SRC_FILE}
            COMMENT "Generating a dSYM file and stripping private symbols")
        install(FILES ${SYM_OUT_FILE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(STRIP_SRC_FILE $<TARGET_FILE:cblite>)
        set(SYM_OUT_FILE $<TARGET_FILE:cblite>.sym)
        set(STRIP_SYMBOLS_PREFIX "" CACHE STRING "The prefix of objcopy and strip command")
        set(OBJCOPY ${STRIP_SYMBOLS_PREFIX}objcopy)
        set(STRIP ${STRIP_SYMBOLS_PREFIX}strip)
        add_custom_command(TARGET cblite POST_BUILD VERBATIM
            COMMAND ${OBJCOPY} --only-keep-debug ${STRIP_SRC_FILE} ${SYM_OUT_FILE}
            COMMAND ${STRIP} --strip-unneeded ${STRIP_SRC_FILE}
            COMMAND ${OBJCOPY} --add-gnu-debuglink=${SYM_OUT_FILE} ${STRIP_SRC_FILE}
            COMMENT "Generating a sym file and stripping private symbols")
        install(FILES ${SYM_OUT_FILE} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    else()
        message(WARNING "Stripping symbols in not supported for ${CMAKE_SYSTEM_NAME}.")
    endif()
endif()

### TESTS:

if(NOT ANDROID)
    add_subdirectory(test)
endif()
