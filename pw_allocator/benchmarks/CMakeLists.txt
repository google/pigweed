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

# Libraries

pw_add_library(pw_allocator.benchmarks.measurements STATIC
  HEADERS
    public/pw_allocator/benchmarks/measurements.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_chrono.system_clock
    pw_containers.intrusive_map
    pw_metric
  SOURCES
    measurements.cc
)

pw_add_library(pw_allocator.benchmarks.benchmark STATIC
  HEADERS
    public/pw_allocator/benchmarks/benchmark.h
    public/pw_allocator/benchmarks/config.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_allocator.benchmarks.measurements
    pw_allocator.block_allocator
    pw_allocator.deallocator
    pw_allocator.fragmentation
    pw_allocator.test_harness
    pw_assert
    pw_chrono.system_clock
    pw_metric
    pw_tokenizer
  SOURCES
    benchmark.cc
)

# Unit tests

pw_add_test(pw_allocator.benchmarks.measurements_test
  SOURCES
    measurements_test.cc
  PRIVATE_DEPS
    pw_allocator.benchmarks.measurements
    pw_metric
  GROUPS
    modules
    pw_allocator
)

pw_add_test(pw_allocator.benchmarks.benchmark_test
  SOURCES
    benchmark_test.cc
  PRIVATE_DEPS
    pw_allocator.benchmarks.benchmark
    pw_allocator.benchmarks.measurements
    pw_allocator.fragmentation
    pw_allocator.testing
    pw_allocator.test_harness
    pw_random
  GROUPS
    modules
    pw_allocator
)
