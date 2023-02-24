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

#include <algorithm>
#include <array>
#include <cstring>

#include "gtest/gtest.h"
#include "pw_tokenizer/tokenize.h"

namespace pw {
namespace tokenizer {
namespace {

template <size_t kSize>
uint32_t TestHash(const char (&str)[kSize])
    PW_NO_SANITIZE("unsigned-integer-overflow") {
  static_assert(kSize > 0u, "Must have at least a null terminator");

  static constexpr uint32_t k65599HashConstant = 65599u;

  // The length is hashed as if it were the first character.
  uint32_t hash = kSize - 1;
  uint32_t coefficient = k65599HashConstant;

  size_t length =
      std::min(static_cast<size_t>(PW_TOKENIZER_CFG_C_HASH_LENGTH), kSize - 1);

  // Hash all of the characters in the string as unsigned ints.
  // The coefficient calculation is done modulo 0x100000000, so the unsigned
  // integer overflows are intentional.
  for (size_t i = 0; i < length; ++i) {
    hash += coefficient * str[i];
    coefficient *= k65599HashConstant;
  }

  return hash;
}

TEST(TokenizeStringLiteral, EmptyString_IsZero) {
  constexpr pw_tokenizer_Token token = PW_TOKENIZE_STRING("");
  EXPECT_EQ(0u, token);
}

TEST(TokenizeStringLiteral, String_MatchesHash) {
  constexpr uint32_t token = PW_TOKENIZE_STRING("[:-)");
  EXPECT_EQ(TestHash("[:-)"), token);
}

constexpr uint32_t kGlobalToken = PW_TOKENIZE_STRING(">:-[]");

TEST(TokenizeStringLiteral, GlobalVariable_MatchesHash) {
  EXPECT_EQ(TestHash(">:-[]"), kGlobalToken);
}

class TokenizeToBuffer : public ::testing::Test {
 public:
  TokenizeToBuffer() : buffer_{} {}

 protected:
  uint8_t buffer_[64];
};

}  // namespace
}  // namespace tokenizer
}  // namespace pw
