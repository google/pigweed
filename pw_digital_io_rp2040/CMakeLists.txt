# Copyright 2023 The Pigweed Authors
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

# This target provides the backend for pw::digital_io
pw_add_library(pw_digital_io_rp2040 INTERFACE
  HEADERS
    public/pw_digital_io_rp2040/digital_io.h
  PUBLIC_INCLUDES
    public
  SOURCES
    digital_io.cc
  PUBLIC_DEPS
    pw_digital_io
    pw_status
    pw_third_party.rp2040
)

pw_add_test(pw_digital_io_rp2040.digital_io_test
  SOURCES
    digital_io_test.cc
  PRIVATE_DEPS
    pico_stdlib
    pw_digital_io
  GROUPS
    modules
    pw_digital_io_rp2040
)
