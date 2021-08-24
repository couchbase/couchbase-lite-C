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

include("${CMAKE_CURRENT_LIST_DIR}/Toolchain-cross-arm64.cmake")
set(CMAKE_C_FLAGS "-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard ${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 3")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "Flags for Raspberry PI 3+")