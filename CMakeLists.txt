# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0077 NEW)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#-------------------------------------------------------------------------------
# Project configuration
#-------------------------------------------------------------------------------

project(iree CXX)
set(IREE_IDE_FOLDER IREE)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(IREE_ENABLE_DEBUG "Enables debugging of the VM." ON)
option(IREE_ENABLE_TRACING "Enables WTF tracing." OFF)

option(IREE_BUILD_TESTS "Builds IREE unit tests." ON)
option(IREE_BUILD_DEBUGGER "Builds the IREE debugger app." OFF)

#-------------------------------------------------------------------------------
# IREE-specific CMake configuration
#-------------------------------------------------------------------------------

list(APPEND CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_LIST_DIR}/build_tools/cmake/
  ${CMAKE_CURRENT_LIST_DIR}/third_party/abseil-cpp/absl/copts/
  ${CMAKE_CURRENT_LIST_DIR}/third_party/flatbuffers/CMake/
)

include(iree_macros)
include(iree_copts)
include(iree_cc_library)
include(iree_cc_test)

#-------------------------------------------------------------------------------
# Third-party dependencies
#-------------------------------------------------------------------------------

include(external_cc_library)
include(flatbuffer_cc_library)

add_subdirectory(build_tools/third_party/ruy EXCLUDE_FROM_ALL)
add_subdirectory(build_tools/third_party/vulkan_memory_allocator EXCLUDE_FROM_ALL)

add_subdirectory(third_party/abseil-cpp EXCLUDE_FROM_ALL)
add_subdirectory(third_party/flatbuffers EXCLUDE_FROM_ALL)
add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
add_subdirectory(third_party/vulkan_headers EXCLUDE_FROM_ALL)

#-------------------------------------------------------------------------------
# IREE top-level libraries
#-------------------------------------------------------------------------------

add_subdirectory(iree/base)
add_subdirectory(iree/hal)
add_subdirectory(iree/samples)
add_subdirectory(iree/schemas)
add_subdirectory(iree/tools)
add_subdirectory(iree/vm)
