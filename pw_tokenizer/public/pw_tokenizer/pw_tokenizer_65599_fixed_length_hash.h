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
#include <string_view>

#include "pw_preprocessor/compiler.h"

namespace pw::tokenizer {

// The constant to use when generating the hash. Changing this changes the value
// of all hashes, so do not change it randomly.
inline constexpr uint32_t k65599HashConstant = 65599u;

// Calculates the hash of a string. This function calculates hashes at either
// runtime or compile time in C++ code.
//
// This function only hashes up to a fixed length. Characters beyond that length
// are ignored. Hashing to a fixed length makes it possible to compute this hash
// in a preprocessor macro. To eliminate some collisions, the length of the
// string is hashed as if it were the first character.
//
// This hash is calculated with the following equation, where s is the string
// and k is the maximum hash length:
//
//    H(s, k) = len(s) + 65599 * s[0] + 65599^2 * s[1] + ... + 65599^k * s[k-1]
//
// The hash algorithm is a modified version of the x65599 hash used by the SDBM
// open source project. This hash has the following differences from x65599:
//   - Characters are only hashed up to a fixed maximum string length.
//   - Characters are hashed in reverse order.
//   - The string length is hashed as the first character in the string.
constexpr uint32_t PwTokenizer65599FixedLengthHash(std::string_view string,
                                                   size_t hash_length)
    PW_NO_SANITIZE("unsigned-integer-overflow") {
  // The length is hashed as if it were the first character.
  uint32_t hash = string.size();
  uint32_t coefficient = k65599HashConstant;

  // Hash all of the characters in the string as unsigned ints.
  // The coefficient calculation is done modulo 0x100000000, so the unsigned
  // integer overflows are intentional.
  for (uint8_t ch : string.substr(0, hash_length)) {
    hash += coefficient * ch;
    coefficient *= k65599HashConstant;
  }

  return hash;
}

}  // namespace pw::tokenizer
