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

include(CMakeParseArguments)

# external_cc_library()
#
# CMake function to imitate Bazel's cc_library rule.
# This is used for external libraries (from third_party, etc) that don't live
# in the IREE namespace.
#
# Parameters:
# PACKAGE: Name of the package (overrides actual path)
# NAME: Name of target (see Note)
# ROOT: Path to the source root where files are found
# HDRS: List of public header files for the library
# SRCS: List of source files for the library
# DEPS: List of other libraries to be linked in to the binary targets
# COPTS: List of private compile options
# DEFINES: List of public defines
# INCLUDES: Include directories to add to dependencies
# LINKOPTS: List of link options
# PUBLIC: Add this so that this library will be exported under iree::
# Also in IDE, target will appear in IREE folder while non PUBLIC will be in IREE/internal.
# TESTONLY: When added, this target will only be built if user passes -DIREE_BUILD_TESTS=ON to CMake.
#
# Note:
# By default, external_cc_library will always create a library named ${PACKAGE}_${NAME},
# and alias target ${PACKAGE}::${NAME}. The ${PACKAGE}:: form should always be used.
# This is to reduce namespace pollution.
#
# external_cc_library(
#   PACKAGE
#     some_external_thing
#   NAME
#     awesome
#   ROOT
#     "third_party/foo"
#   HDRS
#     "a.h"
#   SRCS
#     "a.cc"
# )
# external_cc_library(
#   PACKAGE
#     some_external_thing
#   NAME
#     fantastic_lib
#   ROOT
#     "third_party/foo"
#   SRCS
#     "b.cc"
#   DEPS
#     some_external_thing::awesome # not "awesome" !
#   PUBLIC
# )
#
# iree_cc_library(
#   NAME
#     main_lib
#   ...
#   DEPS
#     some_external_thing::fantastic_lib
# )
#
# TODO: Implement "ALWAYSLINK"
function(external_cc_library)
  cmake_parse_arguments(EXTERNAL_CC_LIB
    "PUBLIC;TESTONLY"
    "PACKAGE;NAME;ROOT"
    "HDRS;SRCS;COPTS;DEFINES;LINKOPTS;DEPS;INCLUDES"
    ${ARGN}
  )

  if(NOT EXTERNAL_CC_LIB_TESTONLY OR IREE_BUILD_TESTS)
    # Prefix the library with the package name.
    string(REPLACE "::" "_" _PACKAGE_NAME ${EXTERNAL_CC_LIB_PACKAGE})
    set(_NAME "${_PACKAGE_NAME}_${EXTERNAL_CC_LIB_NAME}")

    # Prefix paths with the root.
    list(TRANSFORM EXTERNAL_CC_LIB_HDRS PREPEND ${EXTERNAL_CC_LIB_ROOT})
    list(TRANSFORM EXTERNAL_CC_LIB_SRCS PREPEND ${EXTERNAL_CC_LIB_ROOT})

    # Check if this is a header-only library.
    # Note that as of February 2019, many popular OS's (for example, Ubuntu
    # 16.04 LTS) only come with cmake 3.5 by default.  For this reason, we can't
    # use list(FILTER...)
    set(_CC_SRCS "${EXTERNAL_CC_LIB_SRCS}")
    foreach(src_file IN LISTS _CC_SRCS)
      if(${src_file} MATCHES ".*\\.(h|inc)")
        list(REMOVE_ITEM _CC_SRCS "${src_file}")
      endif()
    endforeach()
    if("${_CC_SRCS}" STREQUAL "")
      set(EXTERNAL_CC_LIB_IS_INTERFACE 1)
    else()
      set(EXTERNAL_CC_LIB_IS_INTERFACE 0)
    endif()

    if(NOT EXTERNAL_CC_LIB_IS_INTERFACE)
      add_library(${_NAME} STATIC "")
      target_sources(${_NAME}
        PRIVATE
          ${EXTERNAL_CC_LIB_SRCS}
          ${EXTERNAL_CC_LIB_HDRS}
      )
      target_include_directories(${_NAME}
        PUBLIC
          "$<BUILD_INTERFACE:${IREE_COMMON_INCLUDE_DIRS}>"
          "$<BUILD_INTERFACE:${EXTERNAL_CC_LIB_INCLUDES}>"
      )
      target_compile_options(${_NAME}
        PRIVATE
          ${EXTERNAL_CC_LIB_COPTS}
          ${IREE_DEFAULT_COPTS}
      )
      target_link_libraries(${_NAME}
        PUBLIC
          ${EXTERNAL_CC_LIB_DEPS}
        PRIVATE
          ${EXTERNAL_CC_LIB_LINKOPTS}
          ${IREE_DEFAULT_LINKOPTS}
      )
      target_compile_definitions(${_NAME}
        PUBLIC
          ${EXTERNAL_CC_LIB_DEFINES}
      )

      # Add all external targets to a a folder in the IDE for organization.
      if(EXTERNAL_CC_LIB_PUBLIC)
        set_property(TARGET ${_NAME} PROPERTY FOLDER third_party)
      elseif(EXTERNAL_CC_LIB_TESTONLY)
        set_property(TARGET ${_NAME} PROPERTY FOLDER third_party/test)
      else()
        set_property(TARGET ${_NAME} PROPERTY FOLDER third_party/internal)
      endif()

      # INTERFACE libraries can't have the CXX_STANDARD property set
      set_property(TARGET ${_NAME} PROPERTY CXX_STANDARD ${IREE_CXX_STANDARD})
      set_property(TARGET ${_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
    else()
      # Generating header-only library
      add_library(${_NAME} INTERFACE)
      target_include_directories(${_NAME}
        INTERFACE
          "$<BUILD_INTERFACE:${IREE_COMMON_INCLUDE_DIRS}>"
          "$<BUILD_INTERFACE:${EXTERNAL_CC_LIB_INCLUDES}>"
      )
      target_compile_options(${_NAME}
        INTERFACE
          ${EXTERNAL_CC_LIB_COPTS}
          ${IREE_DEFAULT_COPTS}
      )
      target_link_libraries(${_NAME}
        INTERFACE
          ${EXTERNAL_CC_LIB_DEPS}
          ${EXTERNAL_CC_LIB_LINKOPTS}
          ${IREE_DEFAULT_LINKOPTS}
      )
      target_compile_definitions(${_NAME}
        INTERFACE
          ${EXTERNAL_CC_LIB_DEFINES}
      )
    endif()

    add_library(${EXTERNAL_CC_LIB_PACKAGE}::${EXTERNAL_CC_LIB_NAME} ALIAS ${_NAME})
  endif()
endfunction()
