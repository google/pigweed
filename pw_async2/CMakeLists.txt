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
