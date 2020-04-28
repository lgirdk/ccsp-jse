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

project(jst
	VERSION 1.0.0)

enable_testing()

option(BUILD_RDK "BUILD_RDK" OFF)

# default to Release build
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# add duktape recommended cflags and cmdline options we need
set (CMAKE_C_FLAGS
  "${CMAKE_C_FLAGS} \
  -Wall -Wextra -fstrict-aliasing -fomit-frame-pointer -std=gnu99 \
  -DDUK_USE_PROMISE_BUILTIN \
  -DDUK_CMDLINE_PRINTALERT_SUPPORT \
  -DDUK_CMDLINE_CONSOLE_SUPPORT \
  -DDUK_CMDLINE_LOGGING_SUPPORT \
  -DDUK_CMDLINE_MODULE_SUPPORT")

if(BUILD_RDK)
  message(STATUS, "rdk build")
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_ANSC_LINUX -D_ANSC_USER -D_ANSC_LITTLE_ENDIAN_ -DBUILD_RDK")
else(BUILD_RDK)
  message(STATUS, "generic build")
endif(BUILD_RDK)

# in keeping with the small footprint goal, optimize for size (-Os)
set(CMAKE_C_FLAGS_RELEASE "-Os")

set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fno-inline")

include_directories(
	source
  source/duktape)

set(JST_SOURCES
  source/jst_parser.c
  source/jst_session.c
  source/jst_post.c
  source/jst_functions.c
  source/jst_internal.c
  source/jst_extensions.c
  source/duktape/duktape.c
  source/duktape/duk_cmdline.c
  source/duktape/duk_print_alert.c
  source/duktape/duk_console.c
  source/duktape/duk_logging.c
  source/duktape/duk_module_duktape.c)

set(JST_LIBS "-lm -lcrypto -lsso")

if(BUILD_RDK)
  set(JST_SOURCES ${JST_SOURCES} source/jst_cosa.c)
  set(JST_LIBS "${JST_LIBS} -lccsp_common")
endif(BUILD_RDK)

add_executable(jst ${JST_SOURCES})
target_link_libraries(jst ${JST_LIBS})

if(BUILD_RDK)
  install (TARGETS jst
	  RUNTIME DESTINATION sbin)
endif(BUILD_RDK)

if (BUILD_TESTING)
  add_subdirectory(tests)
endif ()

