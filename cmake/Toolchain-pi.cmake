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
    set(RASPBERRY_VERSION 3)
	include("${CMAKE_CURRENT_LIST_DIR}/Toolchain-cross-arm64.cmake")
else()
	include("${CMAKE_CURRENT_LIST_DIR}/Toolchain-cross-armhf.cmake")
endif()

if(RASPBERRY_VERSION VERSION_GREATER 2)
    set(CMAKE_C_FLAGS "-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard ${COMMON_FLAGS}" CACHE STRING "Flags for Raspberry PI 3")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 3+")
else()
    set(CMAKE_C_FLAGS "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard ${COMMON_FLAGS}" CACHE STRING "Flags for Raspberry PI 2")
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 2+")
endif()
