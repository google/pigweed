# Copyright 2025 The Pigweed Authors
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

# DOCSTAG: [contributing-docs-examples]
include("$ENV{PW_ROOT}/pw_build/pigweed.cmake")

pw_add_test(pw_string.examples.build_inlinestring_with_stringbuilder_test
  PRIVATE_DEPS
    pw_assert.check
    pw_string.builder
    pw_string.string
  SOURCES
    build_inlinestring_with_stringbuilder_test.cc
  GROUPS
    modules
    pw_string
)

pw_add_test(pw_string.examples.build_string_in_buffer_test
  PRIVATE_DEPS
    pw_log
    pw_span
    pw_string.builder
    pw_string.string
  SOURCES
    build_inlinestring_with_stringbuilder_test.cc
  GROUPS
    modules
    pw_string
)

pw_add_test(pw_string.examples.known_size_string_test
  PRIVATE_DEPS
    pw_string.string
  SOURCES
    known_size_string_test.cc
  GROUPS
    modules
    pw_string
)
# DOCSTAG: [contributing-docs-examples]
