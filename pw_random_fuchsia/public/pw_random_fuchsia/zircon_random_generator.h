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
#pragma once

#include <zircon/assert.h>
#include <zircon/syscalls.h>

#include <climits>

#include "pw_random/random.h"
#include "pw_span/span.h"

namespace pw::random_fuchsia {

class ZirconRandomGenerator final : public pw::random::RandomGenerator {
 public:
  void Get(pw::ByteSpan dest) override {
    zx_cprng_draw(dest.data(), dest.size());
  }

  void InjectEntropyBits(uint32_t data, uint_fast8_t num_bits) override {
    static_assert(sizeof(data) <= ZX_CPRNG_ADD_ENTROPY_MAX_LEN);

    constexpr uint8_t max_bits = sizeof(data) * CHAR_BIT;
    if (num_bits == 0) {
      return;
    } else if (num_bits > max_bits) {
      num_bits = max_bits;
    }

    // zx_cprng_add_entropy operates on bytes instead of bits, so round up to
    // the nearest byte so that all entropy bits are included.
    const size_t buffer_size = ((num_bits + CHAR_BIT - 1) / CHAR_BIT);
    zx_cprng_add_entropy(&data, buffer_size);
  }
};

}  // namespace pw::random_fuchsia
