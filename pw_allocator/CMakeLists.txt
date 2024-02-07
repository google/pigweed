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

include("$ENV{PW_ROOT}/pw_build/pigweed.cmake")
include("$ENV{PW_ROOT}/pw_allocator/allocator.cmake")

if(pw_allocator_COLLECT_METRICS)
  set(collect_metrics "PW_ALLOCATOR_COLLECT_METRICS=1")
endif()

pw_add_library(pw_allocator.allocator STATIC
  HEADERS
    public/pw_allocator/allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_preprocessor
    pw_result
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
    pw_allocator.allocator
    pw_bytes
    pw_bytes.alignment
    pw_result
    pw_status
  PRIVATE_DEPS
    pw_assert
  SOURCES
    block.cc
)

pw_add_library(pw_allocator.buffer INTERFACE
  HEADERS
    public/pw_allocator/buffer.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(pw_allocator.fallback_allocator INTERFACE
  HEADERS
    public/pw_allocator/fallback_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.metrics
    pw_allocator.tracking_allocator
    pw_metric
    pw_result
    pw_status
    pw_tokenizer
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

pw_add_library(pw_allocator.libc_allocator STATIC
  SOURCES
    libc_allocator.cc
  HEADERS
    public/pw_allocator/libc_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
)

pw_add_library(pw_allocator.metrics STATIC
  PUBLIC_DEFINES
    ${collect_metrics}
  SOURCES
    metrics.cc
  HEADERS
    public/pw_allocator/metrics.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_metric
  PRIVATE_DEPS
    pw_assert
)

pw_add_library(pw_allocator.multiplex_allocator INTERFACE
  HEADERS
    public/pw_allocator/multiplex_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.metrics
    pw_containers.flat_map
    pw_result
    pw_status
    pw_tokenizer
)

pw_add_library(pw_allocator.null_allocator INTERFACE
  HEADERS
    public/pw_allocator/null_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
)

pw_add_library(pw_allocator.simple_allocator INTERFACE
  HEADERS
    public/pw_allocator/simple_allocator.h
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.block
    pw_bytes
    pw_result
    pw_status
)

pw_add_library(pw_allocator.split_free_list_allocator STATIC
  HEADERS
    public/pw_allocator/split_free_list_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.block
    pw_bytes
    pw_result
    pw_status
  PRIVATE_DEPS
    pw_assert
  SOURCES
    split_free_list_allocator.cc
)

pw_add_library(pw_allocator.synchronized_allocator INTERFACE
  HEADERS
    public/pw_allocator/synchronized_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_sync.borrow
)

pw_add_library(pw_allocator.tracking_allocator INTERFACE
  HEADERS
    public/pw_allocator/tracking_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.metrics
    pw_metric
    pw_status
)

pw_add_library(pw_allocator.allocator_testing STATIC
  HEADERS
    public/pw_allocator/allocator_testing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.block
    pw_allocator.buffer
    pw_allocator.simple_allocator
    pw_allocator.tracking_allocator
    pw_bytes
    pw_result
    pw_status
    pw_unit_test
  PRIVATE_DEPS
    pw_assert
  SOURCES
    allocator_testing.cc
)

pw_add_library(pw_allocator.allocator_test_harness STATIC
  HEADERS
    public/pw_allocator/allocator_test_harness.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_containers
    pw_random
  PRIVATE_DEPS
    pw_assert
    pw_third_party.fuchsia.stdcompat
  SOURCES
    allocator_test_harness.cc
)

pw_add_library(pw_allocator.allocator_fuzzing STATIC
  HEADERS
    public/pw_allocator/allocator_fuzzing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator_test_harness
    pw_fuzzer.fuzztest
  SOURCES
    allocator_fuzzing.cc
)

pw_add_test(pw_allocator.allocator_test
  SOURCES
    allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator
    pw_allocator.allocator_testing
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

pw_add_test(pw_allocator.libc_allocator_test
  SOURCES
    libc_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.libc_allocator
    pw_unit_test
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.multiplex_allocator_test
  PRIVATE_DEPS
    pw_allocator.allocator_testing
    pw_allocator.multiplex_allocator
    pw_tokenizer
  SOURCES
    multiplex_allocator_test.cc
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.null_allocator_test
  SOURCES
    null_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.null_allocator
    pw_unit_test
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.simple_allocator_test
  SOURCES
    simple_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.buffer
    pw_allocator.simple_allocator
)

pw_add_test(pw_allocator.split_free_list_allocator_test
  SOURCES
    split_free_list_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator_fuzzing
    pw_allocator.buffer
    pw_allocator.split_free_list_allocator
    pw_containers.vector
    pw_bytes
    pw_fuzzer.fuzztest
    pw_unit_test
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.synchronized_allocator_test
  SOURCES
    synchronized_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator_testing
    pw_allocator.allocator_test_harness
    pw_allocator.synchronized_allocator
    pw_sync.binary_semaphore
    pw_sync.interrupt_spin_lock
    pw_sync.mutex
    pw_thread.test_thread_context
    pw_thread.thread
    pw_thread.thread_core
    pw_thread.yield
    pw_random
    GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.tracking_allocator_test
  SOURCES
    tracking_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator_testing
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.unique_ptr_test
  SOURCES
    unique_ptr_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator
    pw_allocator.allocator_testing
  GROUPS
    modules
    pw_allocator
)
