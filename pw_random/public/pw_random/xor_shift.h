// Copyright 2020 The Pigweed Authors
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

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "pw_assert/check.h"
#include "pw_bytes/span.h"
#include "pw_random/random.h"
#include "pw_span/span.h"
#include "pw_status/status_with_size.h"

namespace pw::random {

/// A random generator based off the
/// [xorshift*](https://en.wikipedia.org/wiki/Xorshift) algorithm.
///
/// The state is represented as an integer that, with each generation, performs
/// exclusive OR (XOR) operations on different left/right bit shifts of itself.
/// The `*` in `xorshift*` refers to a final multiplication that is applied to
/// the output value. The final multiplication is essentially a nonlinear
/// transformation that makes the algorithm stronger than a plain XOR shift.
///
/// Pigweed's implementation augments `xorshift*` with an ability to inject
/// entropy to reseed the generator throughout its lifetime. When entropy is
/// injected, the results of the generator are no longer completely
/// deterministic based on the original seed.
///
/// See also [Xorshift RNGs](https://www.jstatsoft.org/article/view/v008i14)
/// and [An experimental exploration of Marsaglia's xorshift generators,
/// scrambled](https://vigna.di.unimi.it/ftp/papers/xorshift.pdf).
///
/// @warning This random generator is **NOT** cryptographically secure. It
/// incorporates pseudo-random generation to extrapolate any true injected
/// entropy. The distribution is not guaranteed to be uniform.
class XorShiftStarRng64 : public RandomGenerator {
 public:
  XorShiftStarRng64(uint64_t initial_seed) : state_(initial_seed) {}

  /// Populates the destination buffer with a randomly generated value.
  ///
  /// This generator uses entropy-seeded PRNG to never exhaust its random
  /// number pool.
  void Get(ByteSpan dest) final {
    while (!dest.empty()) {
      uint64_t random = Regenerate();
      size_t copy_size = std::min(dest.size_bytes(), sizeof(state_));
      std::memcpy(dest.data(), &random, copy_size);
      dest = dest.subspan(copy_size);
    }
  }

  /// Injects entropy by rotating the state by the number of entropy bits
  /// before XORing the entropy with the current state.
  ///
  /// This technique ensures that seeding the random value with single bits
  /// will progressively fill the state with more entropy.
  void InjectEntropyBits(uint32_t data, uint_fast8_t num_bits) final {
    if (num_bits == 0) {
      return;
    } else if (num_bits > 32) {
      num_bits = 32;
    }

    // Rotate state.
    uint64_t untouched_state = state_ >> (kNumStateBits - num_bits);
    state_ = untouched_state | (state_ << num_bits);
    // Zero-out all irrelevant bits, then XOR entropy into state.
    uint32_t mask =
        static_cast<uint32_t>((static_cast<uint64_t>(1) << num_bits) - 1);
    state_ ^= (data & mask);
  }

 private:
  // Calculate and return the next value based on the "xorshift*" algorithm
  uint64_t Regenerate() {
    // State must be nonzero, or the algorithm will get stuck and always return
    // zero.
    if (state_ == 0) {
      state_--;
    }
    state_ ^= state_ >> 12;
    state_ ^= state_ << 25;
    state_ ^= state_ >> 27;
    return state_ * kMultConst;
  }
  uint64_t state_;
  static constexpr uint8_t kNumStateBits = sizeof(state_) * 8;

  // For information on why this constant was selected, see:
  // https://www.jstatsoft.org/article/view/v008i14
  // http://vigna.di.unimi.it/ftp/papers/xorshift.pdf
  static constexpr uint64_t kMultConst = 0x2545F4914F6CDD1D;
};

}  // namespace pw::random
