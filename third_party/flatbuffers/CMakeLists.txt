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

set(dir_pw_third_party_flatbuffers "" CACHE PATH "Path to the flatbuffers installation.")

if("${dir_pw_third_party_flatbuffers}" STREQUAL "")
  pw_add_error_target(pw_third_party.flatbuffers
    MESSAGE
      "Attempted to use flatbuffers without configuring it. "
      "Variable 'dir_pw_third_party_flatbuffers' is not set"
  )
  return()
endif()


pw_add_library(pw_third_party.flatbuffers INTERFACE
  HEADERS
    "${dir_pw_third_party_flatbuffers}/include/flatbuffers/base.h"
    "${dir_pw_third_party_flatbuffers}/include/flatbuffers/flatbuffers.h"
    "${dir_pw_third_party_flatbuffers}/include/flatbuffers/flexbuffers.h"
    "${dir_pw_third_party_flatbuffers}/include/flatbuffers/stl_emulation.h"
    "${dir_pw_third_party_flatbuffers}/include/flatbuffers/util.h"
  PUBLIC_INCLUDES
    "${dir_pw_third_party_flatbuffers}/include/"
)

