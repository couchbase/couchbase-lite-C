############################################################################
# toolchain-raspberry.cmake
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Based on:
# https://gitlab.linphone.org/BC/public/linphone-cmake-builder/blob/master/toolchains/toolchain-raspberry.cmake
# https://github.com/Pro/raspi-toolchain/blob/master/Toolchain-rpi.cmake

if("$ENV{RASPBERRY_VERSION}" STREQUAL "")
    set(RASPBERRY_VERSION 2)
else()
    set(RASPBERRY_VERSION $ENV{RASPBERRY_VERSION})
endif()

if(64_BIT)
    set(CMAKE_SYSTEM_PROCESSOR "arm64")
    set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
else()
    set(CMAKE_SYSTEM_PROCESSOR "armv7")
    set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)
endif()

if("$ENV{RASPBIAN_ROOTFS}" STREQUAL "")
	message(FATAL_ERROR "Define the RASPBIAN_ROOTFS environment variable to point to the raspbian rootfs.")
else()
    set(SYSROOT_PATH "$ENV{RASPBIAN_ROOTFS}")
endif()

message(STATUS "Using sysroot path: ${SYSROOT_PATH}")

set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSROOT "${SYSROOT_PATH}")
set(CMAKE_FIND_ROOT_PATH "${SYSROOT_PATH}")
set(CMAKE_SYSTEM_NAME "Linux")

set(TOOLCHAIN_CC "${CMAKE_LIBRARY_ARCHITECTURE}-gcc")
set(TOOLCHAIN_CXX "${CMAKE_LIBRARY_ARCHITECTURE}-g++")
set(TOOLCHAIN_LD "${CMAKE_LIBRARY_ARCHITECTURE}-ld")
set(TOOLCHAIN_AR "${CMAKE_LIBRARY_ARCHITECTURE}-ar")
set(TOOLCHAIN_RANLIB "${CMAKE_LIBRARY_ARCHITECTURE}-ranlib")
set(TOOLCHAIN_STRIP "${CMAKE_LIBRARY_ARCHITECTURE}-strip")
set(TOOLCHAIN_NM "${CMAKE_LIBRARY_ARCHITECTURE}-nm")

set(CMAKE_C_COMPILER ${TOOLCHAIN_CC})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_CXX})

set(LIB_DIRS 
    "${SYSROOT_PATH}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
    "${SYSROOT_PATH}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
)

set(COMMON_FLAGS "-I${SYSROOT_PATH}/usr/include -I${SYSROOT_PATH}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}")
FOREACH(LIB ${LIB_DIRS})
    set(COMMON_FLAGS "${COMMON_FLAGS} -L${LIB} -Wl,-rpath-link,${LIB}")
ENDFOREACH()

set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${SYSROOT_PATH}/usr/lib/${TOOLCHAIN_HOST}")

if(NOT 64_BIT)
    if(RASPBERRY_VERSION VERSION_GREATER 2)
        set(CMAKE_C_FLAGS "-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard ${COMMON_FLAGS}" CACHE STRING "Flags for Raspberry PI 3")
        set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 3")
    else()
        set(CMAKE_C_FLAGS "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard ${COMMON_FLAGS}" CACHE STRING "Flags for Raspberry PI 2")
        set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 2")
    endif()
else()
    set(CMAKE_C_FLAGS "${COMMON_FLAGS}" CACHE STRING "Flags for Raspberry PI 64-bit")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 64-bit")
endif()

set(CMAKE_FIND_ROOT_PATH "${CMAKE_INSTALL_PREFIX};${CMAKE_PREFIX_PATH};${CMAKE_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
