# Copyright 2024 The Pigweed Authors
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

set(dir_pw_third_party_abseil_cpp "" CACHE PATH
    "Path to the Abseil-C++ installation.")
option(pw_third_party_abseil_cpp_ADD_SUBDIRECTORY
    "Whether to add the dir_pw_third_party_abseil_cpp subdirectory" OFF)

if("${dir_pw_third_party_abseil_cpp}" STREQUAL "")
  return()
endif()

if(pw_third_party_abseil_cpp_ADD_SUBDIRECTORY)
  add_subdirectory("${dir_pw_third_party_abseil_cpp}" third_party/abseil-cpp)
endif()
