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

pw_add_library(pw_bytes STATIC
  HEADERS
    public/pw_bytes/array.h
    public/pw_bytes/byte_builder.h
    public/pw_bytes/endian.h
    public/pw_bytes/span.h
    public/pw_bytes/suffix.h
    public/pw_bytes/units.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes.bit
    pw_containers.iterator
    pw_polyfill
    pw_preprocessor
    pw_span
    pw_status
  SOURCES
    byte_builder.cc
)

pw_add_library(pw_bytes.alignment STATIC
  HEADERS
    public/pw_bytes/alignment.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_numeric.checked_arithmetic
    pw_third_party.fuchsia.stdcompat
  SOURCES
    alignment.cc
)

pw_add_library(pw_bytes.bit INTERFACE
  HEADERS
    public/pw_bytes/bit.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_third_party.fuchsia.stdcompat
)

pw_add_library(pw_bytes.packed_ptr INTERFACE
  HEADERS
    public/pw_bytes/packed_ptr.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_third_party.fuchsia.stdcompat
)

pw_add_test(pw_bytes.alignment_test
  SOURCES
    alignment_test.cc
  PRIVATE_DEPS
    pw_bytes.alignment
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.array_test
  SOURCES
    array_test.cc
  PRIVATE_DEPS
    pw_bytes
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.bit_test
  SOURCES
    bit_test.cc
  PRIVATE_DEPS
    pw_bytes.bit
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.byte_builder_test
  SOURCES
    byte_builder_test.cc
  PRIVATE_DEPS
    pw_bytes
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.endian_test
  SOURCES
    endian_test.cc
  PRIVATE_DEPS
    pw_bytes
    pw_unit_test.constexpr
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.packed_ptr_test
  SOURCES
    packed_ptr_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_bytes.packed_ptr
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.span_test
  SOURCES
    span_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_bytes
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.suffix_test
  SOURCES
    suffix_test.cc
  PRIVATE_DEPS
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_bytes
  GROUPS
    modules
    pw_bytes
)

pw_add_test(pw_bytes.units_test
  SOURCES
    units_test.cc
  PRIVATE_DEPS
    pw_bytes
  GROUPS
    modules
    pw_bytes
)
