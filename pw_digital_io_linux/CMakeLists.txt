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

# This target provides the backend for pw::digital_io
pw_add_library(pw_digital_io_linux INTERFACE
  HEADERS
    public/pw_digital_io_linux/digital_io.h
    public/pw_digital_io_linux/internal/owned_fd.h
  PUBLIC_INCLUDES
    public
  SOURCES
    digital_io.cc
  PUBLIC_DEPS
    pw_digital_io
    pw_status
)

# TODO(b/328262654): Move this to a more appropriate module.
pw_add_library(pw_digital_io_linux.mock_vfs STATIC
  HEADERS
    mock_vfs.h
  SOURCES
    mock_vfs.cc
  PRIVATE_DEPS
    pw_log

pw_add_test(pw_digital_io_linux.digital_io_test
  SOURCES
    digital_io_test.cc
  PRIVATE_DEPS
    pw_digital_io
    pw_digital_io_linux.mock_vfs
  GROUPS
    modules
    pw_digital_io_linux
)
