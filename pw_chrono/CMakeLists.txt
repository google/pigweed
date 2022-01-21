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

pw_add_module_library(pw_chrono.epoch
  HEADERS
    public/pw_chrono/epoch.h
  PUBLIC_INCLUDES
    public
)

pw_add_facade(pw_chrono.system_clock
  HEADERS
    public/pw_chrono/internal/system_clock_macros.h
    public/pw_chrono/system_clock.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_chrono.epoch
    pw_preprocessor
  SOURCES
    system_clock.cc
)

pw_add_facade(pw_chrono.system_timer
  HEADERS
    public/pw_chrono/system_timer.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_function
)

# Dependency injectable implementation of pw::chrono::SystemClock::Interface.
pw_add_module_library(pw_chrono.simulated_system_clock
  HEADERS
    public/pw_chrono/simulated_system_clock.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_sync.interrupt_spin_lock
)

# TODO(ewout): Renable this once we've resolved the backend variable definition
# ordering issue, likely by mirroring GN's definition of variables in external
# files which can be imported where needed.
# if((NOT "${pw_chrono.system_clock_BACKEND}"
#     STREQUAL "pw_chrono.system_clock.NO_BACKEND_SET") AND
#    (NOT "${pw_sync.interrupt_spin_lock_BACKEND}"
#     STREQUAL "pw_sync.interrupt_spin_lock.NO_BACKEND_SET"))
#   pw_add_test(pw_chrono.simulated_system_clock_test
#     SOURCES
#       simulated_system_clock_test.cc
#     DEPS
#       pw_chrono.simulated_system_clock
#     GROUPS
#       modules
#       pw_chrono
#   )
# endif()

if(NOT "${pw_chrono.system_clock_BACKEND}"
   STREQUAL "pw_chrono.system_clock.NO_BACKEND_SET")
  pw_add_test(pw_chrono.system_clock_facade_test
    SOURCES
      system_clock_facade_test.cc
      system_clock_facade_test_c.c
    DEPS
      pw_chrono.system_clock
      pw_preprocessor
    GROUPS
      modules
      pw_chrono
  )
endif()

if(NOT "${pw_chrono.system_timer_BACKEND}"
   STREQUAL "pw_chrono.system_timer.NO_BACKEND_SET")
  pw_add_test(pw_chrono.system_timer_facade_test
    SOURCES
      system_timer_facade_test.cc
    DEPS
      pw_chrono.system_timer
      pw_sync.thread_notification
    GROUPS
      modules
      pw_chrono
  )
endif()
