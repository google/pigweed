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

include("$ENV{PW_ROOT}/pw_build/pigweed.cmake")

if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  pw_add_test(pw_async2.examples.basic
    SOURCES
      basic.cc
    PRIVATE_DEPS
      pw_allocator.testing
      pw_async2.coro
  )

  pw_add_test(pw_async2.examples.coro_blinky_loop
    SOURCES
      coro_blinky_loop.cc
    PRIVATE_DEPS
      pw_allocator.testing
      pw_async2.coro
      pw_async2.dispatcher
      pw_async2.time_provider
      pw_async2.simulated_time_provider
      pw_chrono.system_clock
  )

  pw_add_test(pw_async2.examples.once_send_recv
    SOURCES
      once_send_recv.cc
    PRIVATE_DEPS
      pw_allocator.testing
      pw_async2.coro
      pw_async2.once_sender
  )
endif()

add_executable(pw_async2.interrupt EXCLUDE_FROM_ALL
  interrupt.cc
)

target_link_libraries(pw_async2.interrupt
  PRIVATE
    pw_async2.dispatcher
    pw_async2.pend_func_task
    pw_containers.inline_queue
    pw_log
    pw_sync.interrupt_spin_lock
)

pw_add_test(pw_async2.examples.unit_test
  SOURCES
    unit_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_async2.dispatcher
    pw_async2.pend_func_task
)
