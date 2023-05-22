# Copyright 2021 The Pigweed Authors
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

pw_add_library(pw_sync_zephyr.mutex_backend INTERFACE
  HEADERS
    public/pw_sync_zephyr/mutex_inline.h
    public/pw_sync_zephyr/mutex_native.h
    public_overrides/pw_sync_backend/mutex_inline.h
    public_overrides/pw_sync_backend/mutex_native.h
  PUBLIC_INCLUDES
    public
    public_overrides
  PUBLIC_DEPS
    pw_sync.mutex.facade
)
pw_zephyrize_libraries_ifdef(
    CONFIG_PIGWEED_SYNC_MUTEX
    pw_sync_zephyr.mutex_backend
)

pw_add_library(pw_sync_zephyr.binary_semaphore_backend STATIC
  HEADERS
    public/pw_sync_zephyr/binary_semaphore_native.h
    public/pw_sync_zephyr/binary_semaphore_inline.h
    public_overrides/pw_sync_backend/binary_semaphore_native.h
    public_overrides/pw_sync_backend/binary_semaphore_inline.h
  PUBLIC_INCLUDES
    public
    public_overrides
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_sync.binary_semaphore.facade
  SOURCES
    binary_semaphore.cc
)
pw_zephyrize_libraries_ifdef(
    CONFIG_PIGWEED_SYNC_BINARY_SEMAPHORE
    pw_sync_zephyr.binary_semaphore_backend
)

pw_add_library(pw_sync_zephyr.interrupt_spin_lock_backend STATIC
  HEADERS
    public/pw_sync_zephyr/interrupt_spin_lock_inline.h
    public/pw_sync_zephyr/interrupt_spin_lock_native.h
    public_overrides/pw_sync_backend/interrupt_spin_lock_inline.h
    public_overrides/pw_sync_backend/interrupt_spin_lock_native.h
  PUBLIC_INCLUDES
    public
    public_overrides
  PUBLIC_DEPS
    pw_sync.interrupt_spin_lock.facade
  SOURCES
    interrupt_spin_lock.cc
)
pw_zephyrize_libraries_ifdef(
    CONFIG_PIGWEED_SYNC_INTERRUPT_SPIN_LOCK
    pw_sync_zephyr.interrupt_spin_lock_backend
)

pw_add_library(pw_sync_zephyr.thread_notification_backend INTERFACE
  HEADERS
    public/pw_sync_zephyr/thread_notification_inline.h
    public/pw_sync_zephyr/thread_notification_native.h
    public_overrides/thread_notification/pw_sync_backend/thread_notification_inline.h
    public_overrides/thread_notification/pw_sync_backend/thread_notification_native.h
  PUBLIC_INCLUDES
    public
    public_overrides/thread_notification
  PUBLIC_DEPS
    pw_sync.binary_semaphore.facade
    pw_sync.thread_notification.facade
)
pw_zephyrize_libraries_ifdef(
    CONFIG_PIGWEED_SYNC_THREAD_NOTIFICATION
    pw_sync_zephyr.thread_notification_backend
)

pw_add_library(pw_sync_zephyr.timed_thread_notification_backend INTERFACE
  HEADERS
    public/pw_sync_zephyr/timed_thread_notification_inline.h
    public_overrides/timed_thread_notification/pw_sync_backend/timed_thread_notification_inline.h
  PUBLIC_INCLUDES
    public
    public_overrides/timed_thread_notification
  PUBLIC_DEPS
    pw_sync.binary_semaphore.facade
    pw_sync.timed_thread_notification.facade
)

pw_zephyrize_libraries_ifdef(
    CONFIG_PIGWEED_SYNC_TIMED_THREAD_NOTIFICATION
    pw_sync_zephyr.timed_thread_notification_backend
)
