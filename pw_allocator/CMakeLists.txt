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

# Libraries

pw_add_library(pw_allocator.allocator STATIC
  HEADERS
    public/pw_allocator/allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.deallocator
    pw_result
  SOURCES
    allocator.cc
)

pw_add_library(pw_allocator.allocator_as_pool STATIC
  HEADERS
    public/pw_allocator/allocator_as_pool.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.pool
    pw_status
  SOURCES
    allocator_as_pool.cc
)

pw_add_library(pw_allocator.best_fit_block_allocator INTERFACE
  HEADERS
    public/pw_allocator/best_fit_block_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block_allocator_base
)

pw_add_library(pw_allocator.block STATIC
  HEADERS
    public/pw_allocator/block.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.buffer
    pw_assert
    pw_bytes
    pw_bytes.alignment
    pw_result
    pw_status
  PRIVATE_DEPS
    pw_allocator.buffer
    pw_assert
  SOURCES
    block.cc
)

# TODO(b/326509341): Remove when all customers depend on the correct targets.
pw_add_library(pw_allocator.block_allocator INTERFACE
  HEADERS
    public/pw_allocator/block_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.first_fit_block_allocator
    pw_allocator.last_fit_block_allocator
    pw_allocator.best_fit_block_allocator
    pw_allocator.worst_fit_block_allocator
    pw_allocator.dual_first_fit_block_allocator
)

# TODO(b/326509341): Rename when all customers depend on the correct targets.
pw_add_library(pw_allocator.block_allocator_base STATIC
  HEADERS
    public/pw_allocator/block_allocator_base.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.block
    pw_bytes.alignment
    pw_result
    pw_status
  PRIVATE_DEPS
    pw_assert
  SOURCES
    block_allocator.cc
)

pw_add_library(pw_allocator.bucket STATIC
  HEADERS
    public/pw_allocator/bucket.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_function
    pw_span
  PRIVATE_DEPS
    pw_assert
  SOURCES
    bucket.cc
)

pw_add_library(pw_allocator.buddy_allocator STATIC
  HEADERS
    public/pw_allocator/buddy_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_alignment
    pw_allocator.allocator
    pw_allocator.bucket
    pw_allocator.buffer
    pw_bytes
    pw_containers.vector
  PRIVATE_DEPS
    pw_allocator.buffer
    pw_assert
  SOURCES
    buddy_allocator.cc
)

pw_add_library(pw_allocator.buffer STATIC
  HEADERS
    public/pw_allocator/buffer.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_bytes
    pw_result
  PRIVATE_DEPS
    pw_assert
    pw_bytes.alignment
  SOURCES
    buffer.cc
)

pw_add_library(pw_allocator.bump_allocator STATIC
  HEADERS
    public/pw_allocator/bump_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_bytes
  PRIVATE_DEPS
    pw_allocator.buffer
  SOURCES
    bump_allocator.cc
)

pw_add_library(pw_allocator.chunk_pool STATIC
  HEADERS
    public/pw_allocator/chunk_pool.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.pool
    pw_bytes
    pw_result
  PRIVATE_DEPS
    pw_allocator.buffer
    pw_assert.check
  SOURCES
    chunk_pool.cc
)

pw_add_library(pw_allocator.deallocator STATIC
  SOURCES
    deallocator.cc
    unique_ptr.cc
  HEADERS
    public/pw_allocator/capability.h
    public/pw_allocator/layout.h
    public/pw_allocator/deallocator.h
    public/pw_allocator/unique_ptr.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_assert
    pw_preprocessor
    pw_result
    pw_status
)

pw_add_library(pw_allocator.dual_first_fit_block_allocator INTERFACE
  HEADERS
    public/pw_allocator/dual_first_fit_block_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block_allocator_base
)

pw_add_library(pw_allocator.fallback_allocator INTERFACE
  HEADERS
    public/pw_allocator/fallback_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_metric
    pw_result
    pw_status
    pw_tokenizer
)

pw_add_library(pw_allocator.first_fit_block_allocator INTERFACE
  HEADERS
    public/pw_allocator/first_fit_block_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block_allocator_base
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

pw_add_library(pw_allocator.last_fit_block_allocator INTERFACE
  HEADERS
    public/pw_allocator/last_fit_block_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block_allocator_base
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

pw_add_library(pw_allocator.null_allocator INTERFACE
  HEADERS
    public/pw_allocator/null_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
)

pw_add_library(pw_allocator.pool INTERFACE
  HEADERS
    public/pw_allocator/pool.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.deallocator
    pw_bytes
    pw_result
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
    public/pw_allocator/metrics.h
    public/pw_allocator/tracking_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_metric
    pw_status
)

pw_add_library(pw_allocator.typed_pool INTERFACE
  HEADERS
    public/pw_allocator/typed_pool.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.chunk_pool
    pw_bytes
    pw_result
)

pw_add_library(pw_allocator.worst_fit_block_allocator INTERFACE
  HEADERS
    public/pw_allocator/worst_fit_block_allocator.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block_allocator_base
)

# Test support

pw_add_library(pw_allocator.testing INTERFACE
  HEADERS
    public/pw_allocator/testing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.allocator
    pw_allocator.block
    pw_allocator.buffer
    pw_allocator.first_fit_block_allocator
    pw_allocator.tracking_allocator
    pw_bytes
    pw_result
    pw_status
    pw_unit_test
  PRIVATE_DEPS
    pw_assert
)

pw_add_library(pw_allocator.block_allocator_testing STATIC
  HEADERS
    public/pw_allocator/block_allocator_testing.h
    public/pw_allocator/testing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.block
    pw_allocator.block_allocator
    pw_unit_test
  PRIVATE_DEPS
    pw_assert
    pw_bytes.alignment
    pw_status
  SOURCES
    block_allocator_testing.cc
)

pw_add_library(pw_allocator.test_harness STATIC
  HEADERS
    public/pw_allocator/test_harness.h
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
    test_harness.cc
)

pw_add_library(pw_allocator.fuzzing STATIC
  HEADERS
    public/pw_allocator/fuzzing.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.test_harness
    pw_fuzzer.fuzztest
  SOURCES
    fuzzing.cc
)

# Tests

pw_add_test(pw_allocator.allocator_as_pool_test
  PRIVATE_DEPS
    pw_allocator.allocator_as_pool
    pw_allocator.testing
  SOURCES
    allocator_as_pool_test.cc
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.allocator_test
  SOURCES
    allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator
    pw_allocator.testing
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.best_fit_block_allocator_test
  SOURCES
    best_fit_block_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.block_allocator_testing
    pw_allocator.best_fit_block_allocator
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

pw_add_test(pw_allocator.buffer_test
  SOURCES
    buffer_test.cc
  PRIVATE_DEPS
    pw_allocator.buffer
    pw_bytes
    pw_result
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.bump_allocator_test
  PRIVATE_DEPS
    pw_allocator.bump_allocator
  SOURCES
    bump_allocator_test.cc
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.chunk_pool_test
  PRIVATE_DEPS
    pw_allocator.chunk_pool
    pw_allocator.testing
  SOURCES
    chunk_pool_test.cc
)

pw_add_test(pw_allocator.buddy_allocator_test
  PRIVATE_DEPS
    pw_allocator.buddy_allocator
  SOURCES
    buddy_allocator_test.cc
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.dual_first_fit_block_allocator_test
  SOURCES
    dual_first_fit_block_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.block_allocator_testing
    pw_allocator.dual_first_fit_block_allocator
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.fallback_allocator_test
  PRIVATE_DEPS
    pw_allocator.testing
    pw_allocator.fallback_allocator
    pw_status
  SOURCES
    fallback_allocator_test.cc
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.first_fit_block_allocator_test
  SOURCES
    first_fit_block_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.block_allocator_testing
    pw_allocator.buffer
    pw_allocator.first_fit_block_allocator
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

pw_add_test(pw_allocator.last_fit_block_allocator_test
  SOURCES
    last_fit_block_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.block_allocator_testing
    pw_allocator.last_fit_block_allocator
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

pw_add_test(pw_allocator.synchronized_allocator_test
  SOURCES
    synchronized_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.testing
    pw_allocator.test_harness
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
    pw_allocator.testing
    pw_allocator.tracking_allocator
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.typed_pool_test
  SOURCES
    typed_pool_test.cc
  PRIVATE_DEPS
    pw_allocator.typed_pool
    pw_bytes.alignment
    pw_unit_test
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.unique_ptr_test
  SOURCES
    unique_ptr_test.cc
  PRIVATE_DEPS
    pw_allocator.allocator
    pw_allocator.testing
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.worst_fit_block_allocator_test
  SOURCES
    worst_fit_block_allocator_test.cc
  PRIVATE_DEPS
    pw_allocator.block_allocator_testing
    pw_allocator.worst_fit_block_allocator
  GROUPS
    modules
    pw_allocator
)
