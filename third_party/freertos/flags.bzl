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
"""Typical FreeRTOS backends and other platform configuration flags."""

FREERTOS_FLAGS = {
    str(Label("//pw_chrono:system_clock_backend")): str(Label("//pw_chrono_freertos:system_clock")),
    str(Label("//pw_chrono:system_timer_backend")): str(Label("//pw_chrono_freertos:system_timer")),
    str(Label("//pw_sync:binary_semaphore_backend")): str(Label("//pw_sync_freertos:binary_semaphore")),
    str(Label("//pw_sync:counting_semaphore_backend")): str(Label("//pw_sync_freertos:counting_semaphore")),
    str(Label("//pw_sync:interrupt_spin_lock_backend")): str(Label("//pw_sync_freertos:interrupt_spin_lock")),
    str(Label("//pw_sync:mutex_backend")): str(Label("//pw_sync_freertos:mutex")),
    str(Label("//pw_sync:thread_notification_backend")): str(Label("//pw_sync_freertos:thread_notification")),
    str(Label("//pw_sync:timed_mutex_backend")): str(Label("//pw_sync_freertos:timed_mutex")),
    str(Label("//pw_sync:timed_thread_notification_backend")): str(Label("//pw_sync_freertos:timed_thread_notification")),
    str(Label("//pw_system:target_hooks_backend")): str(Label("//pw_system:freertos_target_hooks")),
    str(Label("//pw_thread:id_backend")): str(Label("//pw_thread_freertos:id")),
    str(Label("//pw_thread:iteration_backend")): str(Label("//pw_thread_freertos:thread_iteration")),
    str(Label("//pw_thread:sleep_backend")): str(Label("//pw_thread_freertos:sleep")),
    str(Label("//pw_thread:test_thread_context_backend")): str(Label("//pw_thread_freertos:test_thread_context")),
    str(Label("//pw_thread:thread_backend")): str(Label("//pw_thread_freertos:thread")),
    str(Label("//pw_thread:thread_creation_backend")): str(Label("//pw_thread_freertos:creation")),
    str(Label("//pw_thread:yield_backend")): str(Label("//pw_thread_freertos:yield")),
}
