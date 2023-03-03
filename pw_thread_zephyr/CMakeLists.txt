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

pw_add_library(pw_thread_zephyr.sleep STATIC
  HEADERS
    public/pw_thread_zephyr/sleep_inline.h
    sleep_public_overrides/pw_thread_backend/sleep_inline.h
  PUBLIC_INCLUDES
    public
    sleep_public_overrides
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_thread.sleep.facade
  SOURCES
    sleep.cc
  PRIVATE_DEPS
    pw_chrono_zephyr.system_clock
    pw_assert.check
)
