# Copyright 2020 The Pigweed Authors
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

pw_add_module_library(pw_stream
  SOURCES
    memory_stream.cc
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_result
    pw_span
    pw_status
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_STREAM)
  zephyr_link_libraries(pw_stream)
endif()

pw_add_module_library(pw_stream.socket_stream
  SOURCES
    socket_stream.cc
  PUBLIC_DEPS
    pw_stream
)

pw_add_module_library(pw_stream.sys_io_stream
  PUBLIC_DEPS
    pw_stream
    pw_sys_io
)

pw_add_module_library(pw_stream.std_file_stream
  SOURCES
    std_file_stream.cc
  PUBLIC_DEPS
    pw_stream
)

pw_add_test(pw_stream.memory_stream_test
  SOURCES
    memory_stream_test.cc
  DEPS
    pw_stream
  GROUPS
    modules
    pw_stream
)
