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

#include <cstddef>
#include <cstdint>
#include <span>

#include "pw_bytes/span.h"
#include "pw_status/status_with_size.h"

namespace pw::random {

// A random generator uses injected entropy to generate random values. Many of
// the guarantees for this interface are provided at the level of the
// implementations. In general:
//  * DO NOT assume a generator is cryptographically secure.
//  * DO NOT assume uniformity of generated data.
//  * DO assume a generator can be exhausted.
class RandomGenerator {
 public:
  virtual ~RandomGenerator() = default;

  template <class T>
  StatusWithSize GetInt(T& dest) {
    static_assert(std::is_integral<T>::value,
                  "Use Get() for non-integral types");
    return Get({reinterpret_cast<std::byte*>(&dest), sizeof(T)});
  }

  // Populates the destination buffer with a randomly generated value. Returns:
  //  OK - Successfully filled the destination buffer with random data.
  //  RESOURCE_EXHAUSTED - Filled the buffer with the returned number of bytes.
  //    The returned size is number of complete bytes with random data.
  virtual StatusWithSize Get(ByteSpan dest) = 0;

  // Injects entropy into the pool. `data` may have up to 32 bits of random
  // entropy. If the number of bits of entropy is less than 32, entropy is
  // assumed to be stored in the least significant bits of `data`.
  virtual void InjectEntropyBits(uint32_t data, uint_fast8_t num_bits) = 0;

  // Injects entropy into the pool byte-by-byte.
  void InjectEntropy(ConstByteSpan data) {
    for (std::byte b : data) {
      InjectEntropyBits(std::to_integer<uint32_t>(b), /*num_bits=*/8);
    }
  }
};

}  // namespace pw::random
