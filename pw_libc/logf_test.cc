// Copyright 2025 The Pigweed Authors
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

// This tests the target platform's C standard library version of logf.
//
// Note: We have caught real production bugs with these tests. This exercises
// accelerated FPU operations.
#include <array>
#include <cmath>

#include "pw_unit_test/framework.h"

namespace pw {
namespace {

TEST(logf, BasicOne) { EXPECT_FLOAT_EQ(logf(1.0f), 0.0f); }

TEST(logf, BasicE) { EXPECT_FLOAT_EQ(logf(expf(1.0f)), 1.0f); }

TEST(logf, Array) {
  std::array<float, 8> sequence{{
      14.3f,
      25.1f,
      46.4f,
      78.9f,
      14.3f,
      25.1f,
      46.4f,
      78.9f,
  }};
  std::array<float, 8> expected{{
      2.6602595372658615f,
      3.2228678461377385f,
      3.8372994592322094f,
      4.368181227851829f,
      2.6602595372658615f,
      3.2228678461377385f,
      3.8372994592322094f,
      4.368181227851829f,
  }};
  for (size_t i = 0; i < sequence.size(); i++) {
    EXPECT_FLOAT_EQ(logf(sequence[i]), expected[i]);
  }
}

}  // namespace
}  // namespace pw
