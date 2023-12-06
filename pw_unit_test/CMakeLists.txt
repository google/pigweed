# Copyright 2020 The Pigweed Authors
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

pw_add_module_config(pw_unit_test_CONFIG)

pw_add_library(pw_unit_test.config INTERFACE
  HEADERS
    public/pw_unit_test/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_unit_test_CONFIG}
    pw_polyfill
)

add_library(pw_unit_test INTERFACE)

# Temporarily redirect deprecated googletest pointer to new pointer.
if("${pw_unit_test_GOOGLETEST_BACKEND}" STREQUAL "pw_third_party.googletest")
  message(DEPRECATION
    "pw_unit_test_GOOGLETEST_BACKEND is deprecated. Set pw_unit_test_BACKEND "
    "to pw_unit_test.googletest instead."
  )
  target_link_libraries(pw_unit_test
    INTERFACE
      pw_unit_test.googletest
  )
else()
  target_link_libraries(pw_unit_test
    INTERFACE
      "${pw_unit_test_BACKEND}"
  )
endif()

pw_add_library(pw_unit_test.light STATIC
  SOURCES
    framework_light.cc
  PUBLIC_DEPS
    pw_polyfill
    pw_preprocessor
    pw_string
    pw_unit_test.config
  PUBLIC_INCLUDES
    light_public_overrides
    public
    public_overrides
)

pw_add_library(pw_unit_test.googletest STATIC
  SOURCES
    googletest_public_overrides/pw_unit_test/framework_backend.h
  PUBLIC_INCLUDES
    googletest_public_overrides
  PUBLIC_DEPS
    pw_third_party.googletest
)

pw_add_test(pw_unit_test.framework_test
    SOURCES
      framework_test.cc
    PRIVATE_DEPS
      pw_assert
      pw_status
    GROUPS
      pw_unit_test
  )

if(${pw_unit_test_BACKEND} STREQUAL "pw_unit_test.light")
  pw_add_test(pw_unit_test.framework_light_test
    SOURCES
      framework_light_test.cc
    PRIVATE_DEPS
      pw_string
    GROUPS
      pw_unit_test
  )
endif()

pw_add_library(pw_unit_test.static_library_support STATIC
  HEADERS
    public/pw_unit_test/static_library_support.h
  PUBLIC_INCLUDES
    public
  SOURCES
    static_library_support.cc
  PUBLIC_DEPS
    pw_unit_test.light
)

pw_add_library(pw_unit_test.event_handler INTERFACE
  HEADERS
    public/pw_unit_test/event_handler.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_unit_test.googletest_style_event_handler STATIC
  HEADERS
    public/pw_unit_test/googletest_style_event_handler.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_unit_test.event_handler
    pw_preprocessor
  SOURCES
    googletest_style_event_handler.cc
)

if(${pw_unit_test_BACKEND} STREQUAL "pw_unit_test.googletest")
  pw_add_library(pw_unit_test.googletest_test_matchers INTERFACE
    HEADERS
      public/pw_unit_test/googletest_test_matchers.h
    PUBLIC_INCLUDES
      public
    PUBLIC_DEPS
      pw_result
      pw_status
      pw_third_party.googletest
  )
  pw_add_test(pw_unit_test.googletest_test_matchers_test
    SOURCES
      googletest_test_matchers_test.cc
    PRIVATE_DEPS
      pw_unit_test.googletest_test_matchers
    GROUPS
      pw_unit_test
  )
endif()

pw_add_library(pw_unit_test.simple_printing_main STATIC
  SOURCES
    simple_printing_main.cc
    simple_printing_event_handler.cc
  PRIVATE_DEPS
    pw_unit_test
    pw_unit_test.googletest_style_event_handler
    pw_preprocessor
    pw_string
    pw_sys_io
)

pw_add_library(pw_unit_test.logging_event_handler STATIC
  HEADERS
    public/pw_unit_test/logging_event_handler.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_log
    pw_unit_test.googletest_style_event_handler
  SOURCES
    logging_event_handler.cc
)

pw_add_library(pw_unit_test.logging_main STATIC
  SOURCES
    logging_main.cc
  PRIVATE_DEPS
    pw_unit_test.logging_event_handler
    pw_unit_test
)
