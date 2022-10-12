CMAKE_MINIMUM_REQUIRED (VERSION 3.19)
CMAKE_POLICY (VERSION 3.19)

# Tell CMake to use headers / frameworks from SDK inside XCode instead of
# the ones found on the system (for weak linking).  Ignored on non-Apple
SET(CMAKE_OSX_SYSROOT macosx)

# Top-level CMakeLists for Couchbase Lite Core
PROJECT (couchbase-lite-c-build)

# Provide reasonable default for CMAKE_INSTALL_PREFIX
IF (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  SET(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE STRING
      "The install location" FORCE)
  LIST(APPEND CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}")
ENDIF (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

if("${EDITION}" STREQUAL "enterprise")
    message(STATUS "Building Enterprise Edition...")
    set(BUILD_ENTERPRISE ON CACHE BOOL "Set whether or not to build enterprise edition")
else()
    message(STATUS "Building Community Edition because EDITION was set to ${EDITION}...")
endif()

ADD_SUBDIRECTORY (couchbase-lite-c)