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

# Selects the library that provides FreeRTOS. By default, it uses
# `dir_pw_third_party_freertos` to build the FreeRTOS sources.
pw_add_backend_variable(pw_third_party.freertos_BACKEND
  DEFAULT_BACKEND
    pw_third_party.freertos._direct_impl)

# The default `pw_third_party.freertos._direct_impl` backend requires the
# following variables to be set:
set(dir_pw_third_party_freertos "" CACHE PATH
    "Path to the FreeRTOS installation's Source directory. \
     If set, pw_third_party.freertos is provided")
set(pw_third_party_freertos_CONFIG "" CACHE STRING
    "The CMake target which provides the FreeRTOS config header")
set(pw_third_party_freertos_PORT "" CACHE STRING
    "The CMake target which provides the port specific includes and sources")

option(pw_third_party_freertos_DISABLE_TASKS_STATICS
       "Whether to disable statics inside of tasks.c for debug access")
