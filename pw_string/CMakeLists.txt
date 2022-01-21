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

pw_add_module_library(pw_string
  HEADERS
    public/pw_string/format.h
    public/pw_string/internal/length.h
    public/pw_string/string_builder.h
    public/pw_string/to_string.h
    public/pw_string/type_to_string.h
    public/pw_string/util.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_polyfill.span
    pw_preprocessor
    pw_result
    pw_status
  SOURCES
    format.cc
    string_builder.cc
    type_to_string.cc
)
if(Zephyr_FOUND AND CONFIG_PIGWEED_STRING)
  zephyr_link_libraries(pw_string)
endif()

pw_add_test(pw_string.format_test
  SOURCES
    format_test.cc
  DEPS
    pw_string
  GROUPS
    modules
    pw_string
)

pw_add_test(pw_string.string_builder_test
  SOURCES
    string_builder_test.cc
  DEPS
    pw_string
  GROUPS
    modules
    pw_string
)

pw_add_test(pw_string.to_string_test
  SOURCES
    to_string_test.cc
  DEPS
    pw_string
  GROUPS
    modules
    pw_string
)

pw_add_test(pw_string.type_to_string_test
  SOURCES
    type_to_string_test.cc
  DEPS
    pw_string
  GROUPS
    modules
    pw_string
)

pw_add_test(pw_string.util_test
  SOURCES
    util_test.cc
  DEPS
    pw_string
  GROUPS
    modules
    pw_string
)
