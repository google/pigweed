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

pw_add_library(pw_allocator.bucket.base INTERFACE
  HEADERS
    public/pw_allocator/bucket/base.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.config
    pw_allocator.deallocator
    pw_allocator.block.poisonable
)

pw_add_library(pw_allocator.bucket.fast_sorted INTERFACE
  HEADERS
    public/pw_allocator/bucket/fast_sorted.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.bucket.base
    pw_containers.intrusive_multimap
)

pw_add_library(pw_allocator.bucket.sequenced INTERFACE
  HEADERS
    public/pw_allocator/bucket/sequenced.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.bucket.base
    pw_containers.intrusive_list
)

pw_add_library(pw_allocator.bucket.sorted INTERFACE
  HEADERS
    public/pw_allocator/bucket/sorted.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.bucket.base
    pw_containers.intrusive_forward_list
)

pw_add_library(pw_allocator.bucket.unordered INTERFACE
  HEADERS
    public/pw_allocator/bucket/unordered.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.bucket.base
    pw_containers.intrusive_forward_list
)

pw_add_library(pw_allocator.bucket.testing INTERFACE
  HEADERS
    public/pw_allocator/bucket/testing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.bump_allocator
    pw_allocator.buffer
)

pw_add_test(pw_allocator.bucket.fast_sorted_test
  SOURCES
    fast_sorted_test.cc
  PRIVATE_DEPS
    pw_allocator.bucket.fast_sorted
    pw_allocator.bucket.testing
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.bucket.sequenced_test
  SOURCES
    sequenced_test.cc
  PRIVATE_DEPS
    pw_allocator.bucket.sequenced
    pw_allocator.bucket.testing
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.bucket.sorted_test
  SOURCES
    sorted_test.cc
  PRIVATE_DEPS
    pw_allocator.bucket.sorted
    pw_allocator.bucket.testing
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.bucket.unordered_test
  SOURCES
    unordered_test.cc
  PRIVATE_DEPS
    pw_allocator.bucket.unordered
    pw_allocator.bucket.testing
  GROUPS
    modules
    pw_allocator
)
