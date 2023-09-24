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

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(pw_allocator.allocator STATIC
  HEADERS
    public/pw_allocator/allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_status
  SOURCES
    allocator.cc
)

pw_add_library(pw_allocator.block STATIC
  HEADERS
    public/pw_allocator/block.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_span
    pw_status
  SOURCES
    block.cc
)

pw_add_library(pw_allocator.fallback_allocator STATIC
  HEADERS
    public/pw_allocator/fallback_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_assert
    pw_status
  SOURCES
    fallback_allocator.cc
)

pw_add_library(pw_allocator.freelist STATIC
  HEADERS
    public/pw_allocator/freelist.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.vector
    pw_span
    pw_status
  SOURCES
    freelist.cc
)

pw_add_library(pw_allocator.freelist_heap STATIC
  HEADERS
    public/pw_allocator/freelist_heap.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block
    pw_allocator.freelist
  PRIVATE_DEPS
    pw_assert
    pw_log
    pw_span
  SOURCES
    freelist_heap.cc
)

pw_add_library(pw_allocator.allocator_testing STATIC
  HEADERS
    pw_allocator_private/allocator_testing.h
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.block
    pw_assert
    pw_bytes
  SOURCES
    allocator_testing.cc
)

pw_add_test(pw_allocator.allocator_test
  SOURCES
    allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator
    pw_allocator.allocator_testing
    pw_bytes
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.block_test
  SOURCES
    block_test.cc
  PRIVATE_DEPS
    pw_allocator.block
    pw_span
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.fallback_allocator_test
  PRIVATE_DEPS
    pw_allocator.allocator_testing
    pw_allocator.fallback_allocator
    pw_status
  SOURCES
    fallback_allocator_test.cc
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.freelist_test
  SOURCES
    freelist_test.cc
  PRIVATE_DEPS
    pw_allocator.freelist
    pw_span
    pw_status
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.freelist_heap_test
  SOURCES
    freelist_heap_test.cc
  PRIVATE_DEPS
    pw_allocator.freelist_heap
  GROUPS
    modules
    pw_allocator
)
