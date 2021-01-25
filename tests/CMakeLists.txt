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
include(ExternalProject)
include(FetchContent)
include(GoogleTest)

ExternalProject_Add(
  gtest
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/gtest
  INSTALL_COMMAND "")

ExternalProject_Get_Property(gtest source_dir binary_dir)

if(TEST_COMCAST_WEBUI)
  FetchContent_Declare(
    webui
    GIT_TAG topic/tiny_rdk_top)
  FetchContent_GetProperties(webui)
  if(NOT webui_POPULATED)
    FetchContent_Populate(webui)
  endif()
endif(TEST_COMCAST_WEBUI)

set(CMAKE_C_FLAGS_RELEASE "-Os") 
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fno-inline")

add_library(libgtest IMPORTED STATIC GLOBAL)
add_dependencies(libgtest gtest)
set_target_properties(libgtest PROPERTIES
  "IMPORTED_LOCATION" "${binary_dir}/lib/libgtest.a"
  "IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}")

add_library(libgmock IMPORTED STATIC GLOBAL)
add_dependencies(libgmock gtest)
set_target_properties(libgmock PROPERTIES
  "IMPORTED_LOCATION" "${binary_dir}/lib/libgmock.a"
  "IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}")

include_directories("${source_dir}/googletest/include" "${source_dir}/googlemock/include")

# someone needs to figure out how to run this stuff under valgrind
add_custom_target(test_memcheck
  COMMAND "${CMAKE_CTEST_COMMAND} --force-new-ctest-process --test-action memcheck"
  COMMAND cat "${CMAKE_BINARY_DIR}/Testing/Temporary/MemoryChecker.*.log")

if (CMAKE_VERSION VERSION_LESS "3.1")
  # message("CMAKE ${CMAKE_CXX_COMPILER_ID}")
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "-std=gnu++11 ${CMAKE_CXX_FLAGS}")
  endif ()
else ()
  set (CMAKE_CXX_STANDARD 11)
endif ()

# testGroup.jst_parser
add_executable(
  parser_test
  ../tests/parser_test.cpp 
  ../source/jst_parser.c 
  ../source/jst_internal.c
  ../source/duktape/duktape.c)
target_link_libraries(parser_test libgtest libgmock -pthread)
install(DIRECTORY parser DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if(TEST_COMCAST_WEBUI)
  add_custom_target( extractWebui ALL)
  add_custom_command(TARGET extractWebui PRE_BUILD
     COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/webui
     COMMAND ${CMAKE_COMMAND} -E tar xzf ${CMAKE_CURRENT_BINARY_DIR}/../_deps/webui-src/source/Styles/xb3/jst_tests/webui.tgz
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../_deps/webui-src/source/Styles/xb3/jst_tests/webui.tgz
  COMMENT "extracting webui.tgz"
  VERBATIM)
endif(TEST_COMCAST_WEBUI)

gtest_discover_tests(parser_test)

#to run tests:
# cd build/tests/parser
# ../parser_test
# cd build/tests/webui
# ../parser_test

