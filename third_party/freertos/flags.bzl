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

# LINT.IfChange
FREERTOS_FLAGS = {
    "@pigweed//pw_chrono:system_clock_backend": "@pigweed//pw_chrono_freertos:system_clock",
    "@pigweed//pw_chrono:system_timer_backend": "@pigweed//pw_chrono_freertos:system_timer",
    "@pigweed//pw_sync:binary_semaphore_backend": "@pigweed//pw_sync_freertos:binary_semaphore",
    "@pigweed//pw_sync:interrupt_spin_lock_backend": "@pigweed//pw_sync_freertos:interrupt_spin_lock",
    "@pigweed//pw_sync:mutex_backend": "@pigweed//pw_sync_freertos:mutex",
    "@pigweed//pw_sync:thread_notification_backend": "@pigweed//pw_sync_freertos:thread_notification",
    "@pigweed//pw_sync:timed_thread_notification_backend": "@pigweed//pw_sync_freertos:timed_thread_notification",
    "@pigweed//pw_thread:id_backend": "@pigweed//pw_thread_freertos:id",
    "@pigweed//pw_thread:iteration_backend": "@pigweed//pw_thread_freertos:thread_iteration",
    "@pigweed//pw_thread:sleep_backend": "@pigweed//pw_thread_freertos:sleep",
    "@pigweed//pw_thread:thread_backend": "@pigweed//pw_thread_freertos:thread",
}
# LINT.ThenChange(//.bazelrc)
