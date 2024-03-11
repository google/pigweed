// Copyright 2024 The Pigweed Authors
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

// DOCSTAG: [pw_allocator-examples-custom_allocator-perf_test]
#include <cstdint>

#include "examples/custom_allocator_test_harness.h"
#include "pw_perf_test/perf_test.h"
#include "pw_perf_test/state.h"
#include "pw_random/xor_shift.h"

namespace examples {

void PerformAllocations(pw::perf_test::State& state, uint64_t seed) {
  static CustomAllocatorTestHarness harness;
  pw::random::XorShiftStarRng64 prng(seed);
  while (state.KeepRunning()) {
    harness.GenerateRequest(prng, CustomAllocatorTestHarness::kCapacity);
  }
  harness.Reset();
}

PW_PERF_TEST(CustomAllocatorPerfTest, PerformAllocations, 1);

}  // namespace examples
