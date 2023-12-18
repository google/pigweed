// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <cstddef>

#include "pw_allocator/allocator_test_harness.h"
#include "pw_fuzzer/fuzztest.h"

namespace pw::allocator::test {

/// Returns a FuzzTest domain for producing arbitrary allocator requests.
///
/// This method integrates with FuzzTest to use code coverage to produce guided
/// mutations.
///
/// See https://github.com/google/fuzztest/blob/main/doc/domains-reference.md
///
/// @param  max_size  Size of the largest allocation that can be requested.
fuzzer::Domain<AllocatorRequest> ArbitraryAllocatorRequest(size_t max_size);

/// Returns a FuzzTest domain for producing sequences of arbitrary allocator
/// requests.
///
/// This method can be used to drive an `AllocatorTestHarness` as part of a
/// fuzz test.
///
/// See https://github.com/google/fuzztest/blob/main/doc/domains-reference.md
///
/// @param  max_size  Size of the largest allocation that can be requested.
template <size_t kMaxRequests, size_t kMaxSize>
auto ArbitraryAllocatorRequests() {
  return fuzzer::VectorOf<kMaxRequests>(ArbitraryAllocatorRequest(kMaxSize));
}

}  // namespace pw::allocator::test
