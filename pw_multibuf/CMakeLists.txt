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

pw_add_library(pw_multibuf.chunk STATIC
  HEADERS
    public/pw_multibuf/chunk.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_preprocessor
    pw_span
    pw_sync.mutex
  PRIVATE_DEPS
    pw_assert.check
  SOURCES
    chunk.cc
)

pw_add_library(pw_multibuf.chunk_region_tracker STATIC
  SOURCES
    public/pw_multibuf/chunk_region_tracker.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_bytes
    pw_multibuf.chunk
)

pw_add_test(pw_multibuf.chunk_test
  SOURCES
    chunk_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator_testing
    pw_multibuf.chunk
    pw_multibuf.chunk_region_tracker
  GROUPS
    modules
    pw_multibuf
)

pw_add_test(pw_multibuf.chunk_region_tracker_test
  SOURCES
    chunk_region_tracker_test.cc
  PRIVATE_DEPS
    pw_allocator.simple_allocator
    pw_multibuf.chunk
    pw_multibuf.chunk_region_tracker
    pw_status
  GROUPS
    modules
    pw_multibuf
)

pw_add_library(pw_multibuf STATIC
  HEADERS
    public/pw_multibuf/multibuf.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_multibuf.chunk
  SOURCES
    multibuf.cc
)

pw_add_test(pw_multibuf.multibuf_test
  SOURCES
    multibuf_test.cc
  PRIVATE_DEPS
    pw_multibuf
    pw_multibuf.chunk_region_tracker
    pw_allocator.allocator_testing
  GROUPS
    modules
    pw_multibuf
)
