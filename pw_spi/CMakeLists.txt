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

pw_add_library(pw_spi.initiator INTERFACE
  HEADERS
    public/pw_spi/initiator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_status
)

pw_add_library(pw_spi.responder INTERFACE
  HEADERS
    public/pw_spi/responder.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_status
)

pw_add_library(pw_spi.chip_selector INTERFACE
  HEADERS
    public/pw_spi/chip_selector.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_status
)
