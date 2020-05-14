#
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright 2018 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
cmake_minimum_required(VERSION 2.8)
cmake_policy(SET CMP0048 NEW)

project(jse
	VERSION 1.0.0)

enable_testing()

option(BUILD_RDK "BUILD_RDK" OFF)
option(ENABLE_LIBXML2 "ENABLE_LIBXML2" OFF)

# default to Release build
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# add duktape recommended cflags and cmdline options we need
set (CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} \
  -Wall -Wextra -fstrict-aliasing -fomit-frame-pointer -std=gnu99")

if(BUILD_RDK)
  message(STATUS, "rdk build")
  set (CMAKE_C_FLAGS 
     "${CMAKE_C_FLAGS} \
     -D_ANSC_LINUX \
     -D_ANSC_USER -D_ANSC_LITTLE_ENDIAN_ -DBUILD_RDK")
else(BUILD_RDK)
  message(STATUS, "generic build")
endif(BUILD_RDK)

if(ENABLE_LIBXML2)
  set (CMAKE_C_FLAGS 
    "${CMAKE_C_FLAGS} \
    -DENABLE_LIBXML2 \
    -I/usr/include/libxml2")
endif(ENABLE_LIBXML2)

# in keeping with the small footprint goal, optimize for size (-Os)
set(CMAKE_C_FLAGS_RELEASE "-Os")

set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fno-inline")

include_directories(
  source)

set(JSE_SOURCES
  source/jse_debug.c
  source/jse_common.c
  source/jse_jscommon.c
  source/jse_main.c)

set(JSE_LIBS "-lqdecoder -lduktape -lm")

if(BUILD_RDK)
  set(JSE_SOURCES
    ${JSE_SOURCES}
    source/jse_internal.c
    source/jse_cosa.c)
  set(JSE_LIBS "${JSE_LIBS} -lccsp_common -ldbus-1")
endif(BUILD_RDK)

if(ENABLE_LIBXML2)
  set(JSE_SOURCES
    ${JSE_SOURCES}
    source/jse_xml.c)
  set(JSE_LIBS "${JSE_LIBS} -lxml2")
endif(ENABLE_LIBXML2)

add_executable(jse ${JSE_SOURCES})
target_link_libraries(jse ${JSE_LIBS})

if(BUILD_RDK)
  install (TARGETS jse
	  RUNTIME DESTINATION sbin)
endif(BUILD_RDK)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif(BUILD_TESTING)

