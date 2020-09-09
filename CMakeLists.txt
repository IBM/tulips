# 
# Copyright (c) 2020, International Business Machines
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
# 

project("TULIPS" C CXX)
cmake_minimum_required(VERSION 3.3)

#
# Policies
#

cmake_policy(SET CMP0074 NEW)

#
# Global definitions
#

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Dependencies
#

find_package(PCAP REQUIRED)
find_package(TCLAP REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(IBVerbs)
find_package(ClangFormat)
find_package(ClangTidy)

#
# Options
#

option(TULIPS_TESTS "Enable tests" OFF)
option(TULIPS_DEBUG "Enable debug log" OFF)
option(TULIPS_HAS_HW_CHECKSUM "Target has hardware checksum" OFF)
option(TULIPS_DISABLE_CHECKSUM_CHECK "Disable checksum checks" OFF)
option(TULIPS_HAS_HW_TSO "Target has hardware TSO" OFF)
option(TULIPS_ENABLE_ARP "Enable embedded ARP support" OFF)
option(TULIPS_ENABLE_ICMP "Enable embedded ICMP support" OFF)
option(TULIPS_ENABLE_RAW "Enable embedded ARP support" OFF)
option(TULIPS_ENABLE_LATENCY_MONITOR "Enable client latency monitoring" OFF)
option(TULIPS_IGNORE_INCOMPATIBLE_HW "Ignore when HW lacks features (e.g. TCO)" OFF)

message(STATUS "[ TULIPS OPTIONS BEGIN ]")

if (TULIPS_TESTS)

  set(TULIPS_DEBUG ON)
  set(TULIPS_ENABLE_ARP ON)
  set(TULIPS_ENABLE_ICMP ON)
  set(TULIPS_ENABLE_RAW ON)
  set(TULIPS_IGNORE_INCOMPATIBLE_HW ON)

  add_definitions(-DTULIPS_TESTS)
  add_definitions(-DTULIPS_CLOCK_HAS_OFFSET)
  add_definitions(-DTULIPS_FIFO_RUNTIME_CHECKS)
  add_definitions(-DTULIPS_STACK_RUNTIME_CHECKS)
  add_definitions(-DTULIPS_TRANSPORT_RUNTIME_CHECKS)

  add_definitions(-DTULIPS_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

  message(STATUS "Test mode: ON")

endif (TULIPS_TESTS)

if (TULIPS_DEBUG)
  add_definitions(-DTULIPS_DEBUG)
  message(STATUS "Debug mode: ON")
endif (TULIPS_DEBUG)

if (TULIPS_HAS_HW_CHECKSUM)
  add_definitions(-DTULIPS_HAS_HW_CHECKSUM)
  message(STATUS "Hardware checksum: ON")
endif (TULIPS_HAS_HW_CHECKSUM)

if (TULIPS_DISABLE_CHECKSUM_CHECK)
  add_definitions(-DTULIPS_DISABLE_CHECKSUM_CHECK)
  message(STATUS "Disable checksum check: ON")
endif (TULIPS_DISABLE_CHECKSUM_CHECK)

if (TULIPS_HAS_HW_TSO)
  add_definitions(-DTULIPS_HAS_HW_TSO)
  message(STATUS "TCP segmentation offload: ON")
endif (TULIPS_HAS_HW_TSO)

if (TULIPS_ENABLE_ARP)
  add_definitions(-DTULIPS_ENABLE_ARP)
  add_definitions(-DTULIPS_ENABLE_RAW)
  message(STATUS "ARP protocol support: ON")
endif (TULIPS_ENABLE_ARP)

if (TULIPS_ENABLE_ICMP)
  add_definitions(-DTULIPS_ENABLE_ICMP)
  message(STATUS "ICMP protocol support: ON")
endif (TULIPS_ENABLE_ICMP)

if (TULIPS_ENABLE_RAW)
  add_definitions(-DTULIPS_ENABLE_RAW)
  message(STATUS "RAW packets support: ON")
endif (TULIPS_ENABLE_RAW)

if (TULIPS_ENABLE_LATENCY_MONITOR)
  add_definitions(-DTULIPS_ENABLE_LATENCY_MONITOR)
  message(STATUS "Client latency monitor: ON")
endif (TULIPS_ENABLE_LATENCY_MONITOR)

if (TULIPS_IGNORE_INCOMPATIBLE_HW)
  add_definitions(-DTULIPS_IGNORE_INCOMPATIBLE_HW)
  message(STATUS "Ignore incompatible hardware: ON")
endif (TULIPS_IGNORE_INCOMPATIBLE_HW)

message(STATUS "[ TULIPS OPTIONS END ]")

#
# Flags preferences
#

set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -Wfatal-errors")
set(CMAKE_C_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g3 -O3")

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -Wfatal-errors")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -O3")

#
# Per-compiler options
#

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member")
endif ()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 9.0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-address-of-packed-member")
  endif ()
endif ()

#
# Include directories
#

include_directories(${CMAKE_SOURCE_DIR}/include)

#
# Dependencies
#

find_package(Threads REQUIRED)

#
# Subdirectories
#

add_subdirectory(apps)
add_subdirectory(src)
add_subdirectory(tools)

#
# Testing
#

if (TULIPS_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(tests)
endif (TULIPS_TESTS)

#
# Install all headers
#

install(DIRECTORY include/tulips DESTINATION include PATTERN "*.swp" EXCLUDE)

#
# Formatting
#

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR}
  apps/*.h apps/*.cpp
  include/*.h
  src/*.h src/*.cpp
  tests/*.h tests/*.cpp
  tools/*.h tools/*.cpp)

foreach (SOURCE ${SOURCES})
  string(REGEX REPLACE "[/.]" "_" TARGET ${SOURCE})
  string(TOLOWER ${TARGET} TARGET)
  #
  if (ClangFormat_FOUND)
    add_custom_target(${TARGET}_format COMMAND clang-format -i -style=file ${CMAKE_SOURCE_DIR}/${SOURCE})
    list(APPEND FORMAT_TARGETS ${TARGET}_format)
  endif ()
  #
  if (ClangTidy_FOUND)
    add_custom_target(${TARGET}_tidy COMMAND clang-tidy -quiet -format-style=file -p ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/${SOURCE})
    list(APPEND TIDY_TARGETS ${TARGET}_tidy)
  endif ()
endforeach ()

if (ClangFormat_FOUND)
  add_custom_target(format DEPENDS ${FORMAT_TARGETS})
endif ()

if (ClangTidy_FOUND)
  add_custom_target(tidy DEPENDS ${TIDY_TARGETS})
endif ()
