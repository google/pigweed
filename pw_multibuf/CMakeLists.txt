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

pw_add_library(pw_multibuf INTERFACE
  HEADERS
    public/pw_multibuf/multibuf.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_multibuf.chunk
    pw_multibuf.multibuf_v1
)

pw_add_module_config(pw_multibuf_CONFIG)

pw_add_library(pw_multibuf.config INTERFACE
  HEADERS
    public/pw_multibuf/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    ${pw_multibuf_CONFIG}
)

pw_add_library(pw_multibuf.v1_config INTERFACE
  PUBLIC_DEFINES
    PW_MULTIBUF_VERSION=PW_MULTIBUF_V1
)

pw_add_library(pw_multibuf.chunk STATIC
  HEADERS
    public/pw_multibuf/chunk.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_multibuf.config
    pw_preprocessor
    pw_span
    pw_sync.mutex
  PRIVATE_DEPS
    pw_assert.check
  SOURCES
    chunk.cc
)

pw_add_library(pw_multibuf.header_chunk_region_tracker INTERFACE
  HEADERS
    public/pw_multibuf/header_chunk_region_tracker.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_bytes
    pw_multibuf.chunk
    pw_multibuf.config
)

pw_add_library(pw_multibuf.single_chunk_region_tracker INTERFACE
  HEADERS
    public/pw_multibuf/single_chunk_region_tracker.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_bytes
    pw_multibuf.chunk
    pw_multibuf.config
)

pw_add_test(pw_multibuf.chunk_test
  SOURCES
    chunk_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_multibuf.chunk
    pw_multibuf.header_chunk_region_tracker
    pw_multibuf.v1_config
  GROUPS
    modules
    pw_multibuf
)

pw_add_test(pw_multibuf.header_chunk_region_tracker_test
  SOURCES
    header_chunk_region_tracker_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_multibuf.chunk
    pw_multibuf.header_chunk_region_tracker
    pw_multibuf.v1_config
    pw_status
  GROUPS
    modules
    pw_multibuf
)

pw_add_test(pw_multibuf.single_chunk_region_tracker_test
  SOURCES
    single_chunk_region_tracker_test.cc
  PRIVATE_DEPS
    pw_multibuf.chunk
    pw_multibuf.single_chunk_region_tracker
    pw_multibuf.v1_config
  GROUPS
    modules
    pw_multibuf
)

pw_add_library(pw_multibuf.multibuf_v1 STATIC
  HEADERS
    public/pw_multibuf/multibuf.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_multibuf.chunk
    pw_multibuf.config
    pw_preprocessor
  SOURCES
    multibuf_v1.cc
)

pw_add_test(pw_multibuf.multibuf_v1_test
  SOURCES
    multibuf_v1_test.cc
  PRIVATE_DEPS
    pw_bytes
    pw_multibuf._internal_test_utils
    pw_multibuf.multibuf_v1
    pw_multibuf.v1_config
  GROUPS
    modules
    pw_multibuf
)

pw_add_library(pw_multibuf.allocator STATIC
  HEADERS
    public/pw_multibuf/allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_containers.intrusive_forward_list
    pw_multibuf.config
    pw_multibuf.multibuf_v1
    pw_result
    pw_sync.interrupt_spin_lock
  SOURCES
    allocator.cc
)

pw_add_library(pw_multibuf.allocator_async STATIC
  HEADERS
    public/pw_multibuf/allocator_async.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async2.dispatcher
    pw_async2.poll
    pw_multibuf.allocator
    pw_multibuf.config
  SOURCES
    allocator_async.cc
)

# TODO: https://pwbug.dev/384583239 - Split async into its own test.
pw_add_test(pw_multibuf.allocator_test
  SOURCES
    allocator_test.cc
  PRIVATE_DEPS
    pw_async2.dispatcher
    pw_async2.poll
    pw_multibuf.allocator
    pw_multibuf.allocator_async
    pw_multibuf.v1_config
  GROUPS
    modules
    pw_multibuf
)

pw_add_library(pw_multibuf.simple_allocator STATIC
  HEADERS
    public/pw_multibuf/simple_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_containers.intrusive_list
    pw_multibuf.allocator
    pw_multibuf.config
    pw_multibuf.multibuf_v1
  SOURCES
    simple_allocator.cc
)

pw_add_test(pw_multibuf.simple_allocator_test
  SOURCES
    simple_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_allocator.null_allocator
    pw_multibuf.simple_allocator
    pw_multibuf.v1_config
  GROUPS
    modules
    pw_multibuf
)

pw_add_library(pw_multibuf.stream STATIC
  HEADERS
    public/pw_multibuf/stream.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_multibuf.config
    pw_multibuf.multibuf_v1
    pw_stream
  SOURCES
    stream.cc
)

pw_add_test(pw_multibuf.stream_test
  SOURCES
    stream_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_bytes
    pw_multibuf._internal_test_utils
    pw_multibuf.stream
    pw_multibuf.v1_config
  GROUPS
    modules
    pw_multibuf
)

pw_add_library(pw_multibuf.from_span STATIC
  HEADERS
    public/pw_multibuf/from_span.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_multibuf.multibuf_v1
    pw_function
  SOURCES
    from_span.cc
)

pw_add_test(pw_multibuf.from_span_test
  SOURCES
    from_span_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_multibuf.from_span
    pw_multibuf.v1_config
  SOURCES
    from_span_test.cc
  GROUPS
    modules
    pw_multibuf
)

pw_add_library(pw_multibuf.testing INTERFACE
  HEADERS
    public/pw_multibuf/simple_allocator_for_test.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_allocator.synchronized_allocator
    pw_allocator.testing
    pw_multibuf.config
    pw_multibuf.simple_allocator
    pw_sync.mutex
)

pw_add_library(pw_multibuf._internal_test_utils INTERFACE
  HEADERS
    pw_multibuf_private/test_utils.h
  PUBLIC_DEPS
    pw_assert
    pw_allocator.testing
    pw_bytes
    pw_multibuf.chunk
    pw_multibuf.config
    pw_multibuf.header_chunk_region_tracker
)

## MultiBuf v2

pw_add_library(pw_multibuf.v2_config INTERFACE
  # LINT.IfChange
  PUBLIC_DEFINES
    PW_MULTIBUF_VERSION=PW_MULTIBUF_V2
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)

pw_add_library(pw_multibuf.properties INTERFACE
  # LINT.IfChange
  HEADERS
    public/pw_multibuf/properties.h
  PUBLIC_INCLUDES
    public
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)

pw_add_library(pw_multibuf._iterators INTERFACE
  # LINT.IfChange
  HEADERS
    public/pw_multibuf/chunks.h
    public/pw_multibuf/internal/byte_iterator.h
    public/pw_multibuf/internal/chunk_iterator.h
    public/pw_multibuf/internal/entry.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert.assert
    pw_bytes
    pw_containers.dynamic_deque
    pw_preprocessor
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)

pw_add_test(pw_multibuf.chunk_iterator_test
  # LINT.IfChange
  SOURCES
    chunk_iterator_test.cc
  PRIVATE_DEPS
    pw_multibuf._iterators
    pw_multibuf.iterator_testing
  GROUPS
    modules
    pw_multibuf
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)

pw_add_test(pw_multibuf.byte_iterator_test
  # LINT.IfChange
  SOURCES
    byte_iterator_test.cc
  PRIVATE_DEPS
    pw_multibuf._iterators
    pw_multibuf.iterator_testing
  GROUPS
    modules
    pw_multibuf
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)

pw_add_library(pw_multibuf.iterator_testing INTERFACE
  # LINT.IfChange
  HEADERS
    public/pw_multibuf/internal/iterator_testing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.bump_allocator
    pw_multibuf._iterators
    pw_unit_test
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)

pw_add_library(pw_multibuf.multibuf_v2 STATIC
  # LINT.IfChange
  HEADERS
    public/pw_multibuf/multibuf_v2.h
    public/pw_multibuf/observer.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator
    pw_allocator.null_allocator
    pw_bytes
    pw_containers.dynamic_deque
    pw_multibuf._iterators
    pw_multibuf.properties
  SOURCES
    generic_multibuf.cc
  PRIVATE_DEPS
    pw_assert.check
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)

pw_add_test(pw_multibuf.multibuf_v2_test
  # LINT.IfChange
  SOURCES
    multibuf_v2_test.cc
  PRIVATE_DEPS
    pw_allocator.chunk_pool
    pw_allocator.testing
    pw_assert.check
    pw_bytes
    pw_compilation_testing._pigweed_only_negative_compilation
    pw_multibuf._iterators
    pw_multibuf.chunk
    pw_multibuf.multibuf_v2
    pw_result
    pw_status
  GROUPS
    modules
    pw_multibuf
  # LINT.ThenChange(BUILD.bazel, BUILD.gn)
)
