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
include($ENV{PW_ROOT}/pw_async2/backend.cmake)

pw_add_library(pw_async2.poll INTERFACE
  HEADERS
    public/pw_async2/poll.h
  PUBLIC_DEPS
    pw_polyfill
    pw_string.to_string
    pw_third_party.fuchsia.stdcompat
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_async2.poll_test
  SOURCES
    poll_test.cc
  PRIVATE_DEPS
    pw_async2.poll
)

pw_add_library(pw_async2.dispatcher_base STATIC
  HEADERS
    public/pw_async2/dispatcher_base.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert.assert
    pw_assert.check
    pw_async2.poll
    pw_chrono.system_clock
    pw_sync.interrupt_spin_lock
    pw_sync.lock_annotations
    pw_sync.mutex
    pw_toolchain.no_destructor
  SOURCES
    dispatcher_base.cc
)

pw_add_facade(pw_async2.dispatcher INTERFACE
  BACKEND
    pw_async2.dispatcher_BACKEND
  HEADERS
    public/pw_async2/dispatcher.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async2.dispatcher_base
)

pw_add_test(pw_async2.dispatcher_test
  SOURCES
    dispatcher_test.cc
  PRIVATE_DEPS
    pw_async2.dispatcher
    pw_containers.vector
)

pw_add_test(pw_async2.dispatcher_thread_test
  SOURCES
    dispatcher_thread_test.cc
  PRIVATE_DEPS
    pw_async2.dispatcher
    pw_function
    pw_thread.sleep
    pw_thread.thread
)

pw_add_library(pw_async2.pend_func_task INTERFACE
  HEADERS
    public/pw_async2/pend_func_task.h
  PUBLIC_DEPS
    pw_async2.dispatcher
    pw_function
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_async2.pend_func_task_test
  SOURCES
    pend_func_task_test.cc
  PRIVATE_DEPS
    pw_async2.pend_func_task
)

pw_add_library(pw_async2.pendable_as_task INTERFACE
  HEADERS
    public/pw_async2/pendable_as_task.h
  PUBLIC_DEPS
    pw_async2.dispatcher
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_async2.pendable_as_task_test
  SOURCES
    pendable_as_task_test.cc
  PRIVATE_DEPS
    pw_async2.pendable_as_task
)

pw_add_library(pw_async2.allocate_task INTERFACE
  HEADERS
    public/pw_async2/allocate_task.h
  PUBLIC_DEPS
    pw_async2.dispatcher
    pw_allocator.allocator
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_async2.allocate_task_test
  SOURCES
    allocate_task_test.cc
  PRIVATE_DEPS
    pw_async2.allocate_task
    pw_allocator.testing
)

pw_add_library(pw_async2.once_sender INTERFACE
  HEADERS
    public/pw_async2/once_sender.h
  PUBLIC_DEPS
    pw_async2.dispatcher
    pw_function
  PUBLIC_INCLUDES
    public
)

pw_add_test(pw_async2.once_sender_test
  SOURCES
    once_sender_test.cc
  PRIVATE_DEPS
    pw_async2.once_sender
    pw_containers.vector
)

if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  pw_add_library(pw_async2.coro STATIC
    HEADERS
      public/pw_async2/coro.h
    SOURCES
      coro.cc
    PRIVATE_DEPS
      pw_log
    PUBLIC_DEPS
      pw_allocator.allocator
      pw_async2.dispatcher
      pw_function
    PUBLIC_INCLUDES
      public
  )

  pw_add_test(pw_async2.coro_test
    SOURCES
      coro_test.cc
    PRIVATE_DEPS
      pw_allocator.null_allocator
      pw_allocator.testing
      pw_async2.coro
  )

  pw_add_library(pw_async2.coro_or_else_task INTERFACE
    HEADERS
      public/pw_async2/coro.h
    PUBLIC_DEPS
      pw_async2.coro
      pw_async2.dispatcher
      pw_function
    PUBLIC_INCLUDES
      public
  )

  pw_add_test(pw_async2.coro_or_else_task_test
    SOURCES
      coro_or_else_task_test.cc
    PRIVATE_DEPS
      pw_async2.coro
      pw_async2.coro_or_else_task
      pw_async2.dispatcher
      pw_allocator.null_allocator
      pw_allocator.testing
  )
endif()


pw_add_library(pw_async2.time_provider STATIC
  HEADERS
    public/pw_async2/time_provider.h
  SOURCES
    time_provider.cc
  PUBLIC_DEPS
    pw_async2.dispatcher
    pw_containers.intrusive_list
    pw_sync.interrupt_spin_lock
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_async2.system_time_provider STATIC
  HEADERS
    public/pw_async2/system_time_provider.h
  SOURCES
    system_time_provider.cc
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_async2.time_provider
  PRIVATE_DEPS
    pw_chrono.system_timer
    pw_toolchain.no_destructor
  PUBLIC_INCLUDES
    public
)

if((NOT "${pw_chrono.system_clock_BACKEND}" STREQUAL "") AND
   (NOT "${pw_sync.interrupt_spin_lock_BACKEND}" STREQUAL ""))
  pw_add_test(pw_async2.system_time_provider_test
    SOURCES
      system_time_provider_test.cc
    PRIVATE_DEPS
      pw_async2.system_time_provider
      pw_chrono.system_clock
    GROUPS
      modules
      pw_async2
  )
endif()

pw_add_library(pw_async2.simulated_time_provider INTERFACE
  HEADERS
    public/pw_async2/simulated_time_provider.h
  PUBLIC_DEPS
    pw_async2.time_provider
    pw_sync.interrupt_spin_lock
)

if((NOT "${pw_chrono.system_clock_BACKEND}" STREQUAL "") AND
   (NOT "${pw_sync.interrupt_spin_lock_BACKEND}" STREQUAL ""))
  pw_add_test(pw_async2.simulated_time_provider_test
    SOURCES
      simulated_time_provider_test.cc
    PRIVATE_DEPS
      pw_async2.simulated_time_provider
      pw_chrono.system_clock
    GROUPS
      modules
      pw_async2
  )
endif()

pw_add_library(pw_async2.enqueue_heap_func INTERFACE
  HEADERS
    public/pw_async2/enqueue_heap_func.h
  PUBLIC_DEPS
    pw_async2.dispatcher
)

pw_add_test(pw_async2.enqueue_heap_func_test
  SOURCES
    enqueue_heap_func_test.cc
  PRIVATE_DEPS
    pw_async2.dispatcher
    pw_async2.enqueue_heap_func
  GROUPS
    modules
    pw_async2
)

pw_add_library(pw_async2.join INTERFACE
  HEADERS
    public/pw_async2/join.h
  PUBLIC_DEPS
    pw_async2.dispatcher
)

pw_add_test(pw_async2.join_test
  SOURCES
    join_test.cc
  PRIVATE_DEPS
    pw_async2.dispatcher
    pw_async2.join
  GROUPS
    modules
    pw_async2
)

add_subdirectory(examples)
