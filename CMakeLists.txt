cmake_minimum_required (VERSION 3.9)
cmake_policy(VERSION 3.9)

if(NOT DEFINED CMAKE_OSX_SYSROOT)
    # Tells Mac builds to use the current SDK's headers & libs, not what's in the OS.
    set(CMAKE_OSX_SYSROOT macosx)      # this line MUST be above the 1st "project()" line!
endif()

project (
    CouchbaseLite_C
    VERSION 3.0.0
)

include(CheckIPOSupported)

set(CBL_LIB_VERSION ${CouchbaseLite_C_VERSION})
set(CBL_API_VERSION ${CouchbaseLite_C_VERSION_MAJOR})

### BUILD SETTINGS:

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
option(BUILD_ENTERPRISE "Set whether or not to build enterprise edition" OFF)
option(CODE_COVERAGE_ENABLED "Set whether or not code coverage report should be generated" OFF)

if(CMAKE_COMPILER_IS_GNUCC)
    # Suppress an annoying note about GCC 7 ABI changes
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
endif()

if(CODE_COVERAGE_ENABLED)
   message("Code coverage enabled, forcing sanitizers off")
   if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
   elseif(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
     message(WARN " Code coverage not supported for non-debug builds")
   else()
     message(WARN " Code coverage only supported on Clang")
   endif()
endif()

if(BUILD_ENTERPRISE)
    message("Building Enterprise Edition")
    set(COUCHBASE_ENTERPRISE 1)                     # For generating CBL_Edition.h
else()
    message("Building Community Edition")
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/include/cbl/CBL_Edition.h.in"
    "${PROJECT_BINARY_DIR}/generated_headers/public/cbl/CBL_Edition.h"
)

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

include_directories(
    "vendor/couchbase-lite-core/C/include"
    "vendor/couchbase-lite-core/C/Cpp_include"
    "vendor/couchbase-lite-core/LiteCore/Support"
    "vendor/couchbase-lite-core/vendor/fleece/API"
    "vendor/couchbase-lite-core/vendor/fleece/Fleece/Support"
)

if(!MSVC)
    set_source_files_properties(${C_SRC} PROPERTIES COMPILE_FLAGS -Wno-return-type-c-linkage)
endif()

### BUILDING THE LIBRARY:

set_platform_source_files(RESULT PLATFORM_SRC)
set(
    ALL_SRC_FILES
    src/CBLBase_CAPI.cc
    src/CBLBlob_CAPI.cc
    src/CBLDatabase.cc
    src/CBLDatabase_CAPI.cc
    src/CBLDocument.cc
    src/CBLDocument_CAPI.cc
    src/CBLLog.cc
    src/CBLQuery_CAPI.cc
    src/CBLReplicator_CAPI.cc
    src/ConflictResolver.cc
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cbl
    src
    vendor/couchbase-lite-core/C
    ${PROJECT_BINARY_DIR}/generated_headers/private/cbl
    ${PLATFORM_INCLUDE}
)

target_link_libraries(
    cblite-static INTERFACE
    LiteCoreStatic
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

target_include_directories(
    cblite
    PRIVATE
    src
    vendor/couchbase-lite-core/C
    ${PROJECT_BINARY_DIR}/generated_headers/private/cbl
    ${PLATFORM_INCLUDE}
)

set(CBL_LIBRARIES_PRIVATE
    ${WHOLE_LIBRARY_FLAG}
    cblite-static
    ${NO_WHOLE_LIBRARY_FLAG}
    LiteCoreWebSocket
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^armv[67]")
    # C/C++ atomic operations on ARM6/7 emit calls to functions in libatomic
    set(CBL_LIBRARIES_PRIVATE ${CBL_LIBRARIES_PRIVATE}
                              atomic)
endif()

target_link_libraries(cblite PRIVATE ${CBL_LIBRARIES_PRIVATE})

set_dylib_properties()
set_target_properties(cblite PROPERTIES VERSION "${CBL_LIB_VERSION}" SOVERSION "${CBL_API_VERSION}")

## Setting up LTO

if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    check_ipo_supported(RESULT CBLITE_HAS_LTO OUTPUT CBLITE_LTO_ERROR LANGUAGES C CXX)
    if(CBLITE_HAS_LTO)
        set_property(TARGET cblite-static PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        set_property(TARGET cblite PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "Link time optimization not supported on this platform: ${CBLITE_LTO_ERROR}")
    endif()
endif()

## Installation

if(ANDROID)
    set(CMAKE_INSTALL_LIBDIR "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
else()
    set(CMAKE_INSTALL_LIBDIR "lib") # No lib64, stop!
endif()

install(
    TARGETS cblite
    EXPORT CouchbaseLiteTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

file(GLOB C_HEADERS ${PROJECT_SOURCE_DIR}/include/cbl/*.h)
file(GLOB FLEECE_HEADERS ${PROJECT_SOURCE_DIR}/vendor/couchbase-lite-core/vendor/fleece/API/fleece/*.h)

install(EXPORT CouchbaseLiteTargets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CouchbaseLite)

install(FILES ${C_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbl)
install(FILES ${PROJECT_BINARY_DIR}/generated_headers/public/cbl/CBL_Edition.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cbl)
install(FILES ${FLEECE_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fleece)

configure_file(cblite.pc.in cblite.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/cblite.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

include(CMakePackageConfigHelpers)
set(TARGETS_EXPORT_NAME "CouchbaseLiteTargets")
configure_package_config_file(
  cmake/CouchbaseLiteConfig.cmake.in
  ${CMAKE_BINARY_DIR}/CouchbaseLiteConfig.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/CouchbaseLite"
)
write_basic_package_version_file(
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/CouchbaseLite/CouchbaseLiteConfigVersion.cmake
    VERSION ${CBL_LIBRARY_VERSION} COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_BINARY_DIR}/CouchbaseLiteConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CouchbaseLite)

### TESTS:

if(NOT ANDROID)
    add_subdirectory(test)
endif()
