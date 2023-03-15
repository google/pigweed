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

if(CONFIG_PIGWEED_LOG_ZEPHYR)
  pw_add_library(pw_log_zephyr STATIC
    HEADERS
      public/pw_log_zephyr/log_zephyr.h
      public/pw_log_zephyr/config.h
      public_overrides/pw_log_backend/log_backend.h
    PUBLIC_INCLUDES
      public
      public_overrides
    PUBLIC_DEPS
      pw_log.facade
      zephyr_interface
    SOURCES
      log_zephyr.cc
    PRIVATE_DEPS
      pw_preprocessor
  )
  zephyr_link_libraries(pw_log_zephyr)
endif()

if(CONFIG_PIGWEED_LOG_TOKENIZED)
  pw_add_library(pw_log_zephyr.tokenized_handler STATIC
    SOURCES
      pw_log_zephyr_tokenized_handler.cc
    PRIVATE_DEPS
      pw_log_tokenized.handler
      pw_tokenizer
  )
  zephyr_link_libraries(pw_log pw_log_zephyr.tokenized_handler)
  zephyr_include_directories(public_overrides)
endif()
