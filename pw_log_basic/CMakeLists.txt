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

pw_auto_add_simple_module(pw_log_basic
  IMPLEMENTS_FACADE
    pw_log
  PRIVATE_DEPS
    pw_string
    pw_sys_io
)

target_include_directories(pw_log_basic PUBLIC public_overrides)

# TODO(hepler): Declare pw_log_basic as the pw_log backend for now.
add_library(pw_log.backend INTERFACE)
target_link_libraries(pw_log.backend INTERFACE pw_log_basic)
