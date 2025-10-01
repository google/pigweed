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

#include "pw_containers/bitset.h"

#include "pw_assert/assert.h"

namespace examples {

// A BitSet of 8 bits, initialized to 0b10101010.
constexpr pw::BitSet<8> my_bitset1 = pw::BitSet<8>::Of<0b10101010>();

// BitSets can also be initialized with booleans.
constexpr pw::BitSet<4> my_bitset2 =
    pw::BitSet<4>::LittleEndian(true, false, true, false);

constexpr pw::BitSet<8> MergeAndModify() {
  pw::BitSet<8> bits = my_bitset1 | pw::BitSet<8>(my_bitset2);
  PW_ASSERT(bits == pw::BitSet<8>::Of<0b10101111>());

  // Bits can be tested, set, flipped, and reset (cleared).
  PW_ASSERT(!bits.test<6>());
  bits.set<6>();
  PW_ASSERT(bits.test<6>());

  bits.flip();
  PW_ASSERT(bits == pw::BitSet<8>::Of<0b00010000>());

  bits.reset<4>();
  return bits;
}

// BitSet is fully constexpr.
static_assert(MergeAndModify().none());

}  // namespace examples
