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

#include "zircon_random_generator.h"

#include "pw_unit_test/framework.h"

namespace pw_random_zircon {

TEST(ZirconRandomGeneratorTest, Get) {
  // Getting a random number should not crash.
  std::array<std::byte, 4> value = {std::byte{0}};
  ZirconRandomGenerator().Get(value);
}

TEST(ZirconRandomGeneratorTest, InjectEntropyBits) {
  ZirconRandomGenerator rng;
  // Injecting 0 bits of entropy should safely do nothing.
  rng.InjectEntropyBits(/*data=*/1, /*num_bits=*/0);
  // Injecting too many bits should round down to 32 and not crash.
  rng.InjectEntropyBits(/*data=*/1, /*num_bits=*/33);
  // Inject the maximum number of bits.
  rng.InjectEntropyBits(/*data=*/1, /*num_bits=*/32);
  rng.InjectEntropyBits(/*data=*/1, /*num_bits=*/8);
  rng.InjectEntropyBits(/*data=*/1, /*num_bits=*/31);
}

}  // namespace pw_random_zircon
