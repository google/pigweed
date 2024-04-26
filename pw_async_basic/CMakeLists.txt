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

pw_add_library(pw_async_basic.task_backend INTERFACE
  HEADERS
    public/pw_async_basic/task.h
    public_overrides/pw_async_backend/task.h
  PUBLIC_INCLUDES
    public
    public_overrides
  PUBLIC_DEPS
    pw_async.task.facade
    pw_containers.intrusive_list
)

pw_add_library(pw_async_basic.dispatcher_backend STATIC
  HEADERS
    public/pw_async_basic/dispatcher.h
  SOURCES
    dispatcher.cc
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async_basic.task_backend
    pw_async.dispatcher.facade
    pw_containers.intrusive_list
    pw_sync.interrupt_spin_lock
    pw_sync.timed_thread_notification
    pw_thread.thread_core
)

pw_add_library(pw_async_basic.fake_dispatcher_backend STATIC
  HEADERS
    public/pw_async_basic/fake_dispatcher.h
    public_overrides/pw_async_backend/fake_dispatcher.h
  SOURCES
    fake_dispatcher.cc
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async_basic.task_backend
    pw_async.fake_dispatcher.facade
    pw_unit_test
  PRIVATE_DEPS
    pw_log
)
