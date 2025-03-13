# Copyright 2025 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_flatbuffers/flatbuffers.cmake)

if (NOT "${dir_pw_third_party_flatbuffers}" STREQUAL "")
  pw_flatbuffer_library(pw_flatbuffers.simple_test_schema
    SOURCES
      pw_flatbuffers_test_schema/simple_test.fbs
  )

  pw_flatbuffer_library(pw_flatbuffers.simple_test_schema_prefix
    SOURCES
      pw_flatbuffers_test_schema/simple_test.fbs
    PREFIX
      pw_flatbuffers_test_prefix
  )

  pw_flatbuffer_library(pw_flatbuffers.simple_test_schema_no_prefix
    SOURCES
      pw_flatbuffers_test_schema/simple_test.fbs
    STRIP_PREFIX
      pw_flatbuffers_test_schema
  )

  pw_add_test(pw_flatbuffers.simple_test
    SOURCES
      simple_test.cc
    PRIVATE_DEPS
      pw_flatbuffers.simple_test_schema.cpp
    PRIVATE_DEFINES
      NORMAL_TEST=1
    GROUPS
      modules
  )

  pw_add_test(pw_flatbuffers.simple_test_prefix
    SOURCES
      simple_test.cc
    PRIVATE_DEPS
      pw_flatbuffers.simple_test_schema_prefix.cpp
    PRIVATE_DEFINES
      PREFIX_TEST=1
    GROUPS
      modules
  )

  pw_add_test(pw_flatbuffers.simple_test_no_prefix
    SOURCES
      simple_test.cc
    PRIVATE_DEPS
      pw_flatbuffers.simple_test_schema_no_prefix.cpp
    PRIVATE_DEFINES
      NO_PREFIX_TEST=1
    GROUPS
      modules
  )
endif()
