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

if(Zephyr_FOUND AND CONFIG_PIGWEED_CHRONO_SYSTEM_CLOCK)
  pw_add_module_library(pw_chrono_zephyr.system_clock
    IMPLEMENTS_FACADES
      pw_chrono.system_clock
    PUBLIC_DEPS
      pw_function
  )
  pw_set_backend(pw_chrono.system_clock pw_chrono_zephyr.system_clock)
  zephyr_link_interface(pw_chrono_zephyr.system_clock)
  zephyr_link_libraries(pw_chrono_zephyr.system_clock)
endif()