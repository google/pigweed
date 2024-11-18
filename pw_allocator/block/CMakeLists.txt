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

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(pw_allocator.block.alignable INTERFACE
  HEADERS
    public/pw_allocator/block/alignable.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block.allocatable
    pw_allocator.deallocator
    pw_assert
    pw_bytes.alignment
    pw_status
    pw_third_party.fuchsia.stdcompat
)

pw_add_library(pw_allocator.block.allocatable INTERFACE
  HEADERS
    public/pw_allocator/block/allocatable.h
    public/pw_allocator/block/result.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block.contiguous
    pw_allocator.deallocator
    pw_assert
    pw_bytes.alignment
    pw_status
)

pw_add_library(pw_allocator.block.basic STATIC
  HEADERS
    public/pw_allocator/block/basic.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.config
    pw_assert
    pw_bytes.alignment
    pw_third_party.fuchsia.stdcompat
  SOURCES
    basic.cc
)

pw_add_library(pw_allocator.block.contiguous STATIC
  HEADERS
    public/pw_allocator/block/contiguous.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block.basic
    pw_bytes
    pw_result
    pw_status
    pw_third_party.fuchsia.stdcompat
  SOURCES
    contiguous.cc
)

pw_add_library(pw_allocator.block.iterable INTERFACE
  HEADERS
    public/pw_allocator/block/iterable.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block.contiguous
)

pw_add_library(pw_allocator.block.poisonable STATIC
  HEADERS
    public/pw_allocator/block/poisonable.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block.contiguous
    pw_allocator.config
    pw_third_party.fuchsia.stdcompat
  SOURCES
    poisonable.cc
)

pw_add_library(pw_allocator.block.with_layout INTERFACE
  HEADERS
    public/pw_allocator/block/with_layout.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block.alignable
    pw_allocator.deallocator
    pw_assert
)

pw_add_library(pw_allocator.block.detailed_block INTERFACE
  HEADERS
    public/pw_allocator/block/detailed_block.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block.alignable
    pw_allocator.block.allocatable
    pw_allocator.block.basic
    pw_allocator.block.contiguous
    pw_allocator.block.iterable
    pw_allocator.block.poisonable
    pw_allocator.block.with_layout
    pw_allocator.config
    pw_allocator.deallocator
    pw_assert
    pw_bytes
    pw_preprocessor
    pw_result
    pw_status
)

pw_add_library(pw_allocator.block.testing INTERFACE
  HEADERS
    public/pw_allocator/block/testing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.testing
    pw_assert
    pw_bytes.alignment
    pw_bytes
    pw_result
    pw_third_party.fuchsia.stdcompat
)

pw_add_test(pw_allocator.block.detailed_block_test
  SOURCES
    detailed_block_test.cc
  PRIVATE_DEPS
    pw_allocator.block.detailed_block
    pw_allocator.block.testing
    pw_span
    pw_third_party.fuchsia.stdcompat
  GROUPS
    modules
    pw_allocator
)
