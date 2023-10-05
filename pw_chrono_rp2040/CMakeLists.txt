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

# This target provides the backend for pw::chrono::SystemClock.
pw_add_library(pw_chrono_rp2040.system_clock INTERFACE
  HEADERS
    public/pw_chrono_rp2040/system_clock_config.h
    public/pw_chrono_rp2040/system_clock_inline.h
    public_overrides/pw_chrono_backend/system_clock_config.h
    public_overrides/pw_chrono_backend/system_clock_inline.h
  PUBLIC_INCLUDES
    public
    public_overrides
  PUBLIC_DEPS
    pw_chrono.epoch
    pw_chrono.system_clock.facade
    pw_third_party.rp2040
)

pw_add_test(pw_chrono_rp2040.clock_properties_test
  SOURCES
    clock_properties_test.cc
  PRIVATE_DEPS
    pico_sync
    pw_chrono.system_clock
  GROUPS
    modules
    pw_chrono_rp2040
)
