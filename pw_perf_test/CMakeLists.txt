# Copyright 2022 The Pigweed Authors
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
include($ENV{PW_ROOT}/pw_protobuf_compiler/proto.cmake)

pw_add_module_library(pw_perf_test.duration_unit
  HEADERS
    public/pw_perf_test/internal/duration_unit.h
  PUBLIC_INCLUDES
    public
)

pw_add_facade(pw_perf_test.timer
  HEADERS
    public/pw_perf_test/internal/timer.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_perf_test.duration_unit
)

pw_add_module_library(pw_perf_test
  SOURCES
    perf_test.cc
  HEADERS
    public/pw_perf_test/perf_test.h
  PUBLIC_INCLUDES
    public
)

pw_add_module_library(pw_perf_test.chrono_timer
  IMPLEMENTS_FACADES
    pw_perf_test.timer
  HEADERS
    chrono_public_overrides/pw_perf_test_timer_backend/timer.h
    public/pw_perf_test/internal/chrono_timer_interface.h
  PUBLIC_INCLUDES
    chrono_public_overrides
    public
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_perf_test.duration_unit
)

if(NOT "${pw_perf_test.TIMER_INTERFACE_BACKEND}"
    STREQUAL "pw_perf_test.timer.NO_BACKEND_SET")
  pw_add_test(pw_perf_test.timer_test
    SOURCES
      timer_test.cc
    PRIVATE_DEPS
      pw_perf_test.timer
      pw_thread.sleep
      pw_chrono.system_clock
    GROUPS
      modules
      pw_perf_test
  )
endif()

if(NOT "${pw_perf_test.TIMER_INTERFACE_BACKEND}"
    STREQUAL "pw_chrono.SYSTEM_CLOCK_BACKEND")
  pw_add_test(pw_perf_test.chrono_timer_test
    SOURCES
      chrono_test.cc
    PRIVATE_DEPS
      pw_perf_test.timer
      pw_thread.sleep
      pw_chrono.system_clock
    GROUPS
      modules
      pw_perf_test
  )
endif()
