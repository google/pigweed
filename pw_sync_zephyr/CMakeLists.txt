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

if(CONFIG_PIGWEED_SYNC_MUTEX)
  pw_add_module_library(pw_sync_zephyr.mutex_backend
    IMPLEMENTS_FACADES
      pw_sync.mutex
  )
  pw_set_backend(pw_sync.mutex pw_sync_zephyr.mutex_backend)
  zephyr_link_libraries(pw_sync_zephyr.mutex_backend)
  zephyr_link_interface(pw_sync_zephyr.mutex_backend)
endif()

if(CONFIG_PIGWEED_SYNC_BINARY_SEMAPHORE)
  pw_add_module_library(pw_sync_zephyr.binary_semaphore_backend
    IMPLEMENTS_FACADES
      pw_sync.binary_semaphore
    SOURCES
      binary_semaphore.cc
  )
  pw_set_backend(pw_sync.binary_semaphore pw_sync_zephyr.binary_semaphore_backend)
  zephyr_link_libraries(pw_sync_zephyr.binary_semaphore_backend)
  zephyr_link_interface(pw_sync_zephyr.binary_semaphore_backend)
endif()
