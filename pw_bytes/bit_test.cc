
// Copyright 2022 The Pigweed Authors
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

#include "pw_bytes/bit.h"

#include <cstdint>
#include <cstring>

#include "pw_unit_test/framework.h"

namespace pw::bytes {
namespace {

// SignExtend unsigned integer values into signed integers.
constexpr uint32_t kNegative24Bits = 0x00FACADE;
constexpr int32_t kExtendedNegative24Bits = SignExtend<24>(kNegative24Bits);
static_assert(kExtendedNegative24Bits == static_cast<int32_t>(0xFFFACADE));

constexpr uint32_t kPositive20Bits = 0x00000ACE;
constexpr int32_t kExtendedPositive20Bits = SignExtend<20>(kPositive20Bits);
static_assert(kExtendedPositive20Bits == static_cast<int32_t>(0x00000ACE));

constexpr uint32_t kNegative12Bits = 0x00000ACE;
constexpr int32_t kExtendedNegative12Bits = SignExtend<12>(kNegative12Bits);
static_assert(kExtendedNegative12Bits == static_cast<int32_t>(0xFFFFFACE));

TEST(Endian, NativeIsBigOrLittle) {
  EXPECT_TRUE(endian::native == endian::little ||
              endian::native == endian::big);
}

TEST(Endian, NativeIsCorrect) {
  constexpr uint32_t kInteger = 0x11223344u;
  int8_t bytes[sizeof(kInteger)] = {};
  std::memcpy(bytes, &kInteger, sizeof(kInteger));

  if (endian::native == endian::little) {
    EXPECT_EQ(bytes[0], 0x44);
    EXPECT_EQ(bytes[1], 0x33);
    EXPECT_EQ(bytes[2], 0x22);
    EXPECT_EQ(bytes[3], 0x11);
  } else {
    EXPECT_EQ(bytes[0], 0x11);
    EXPECT_EQ(bytes[1], 0x22);
    EXPECT_EQ(bytes[2], 0x33);
    EXPECT_EQ(bytes[3], 0x44);
  }
}

}  // namespace
}  // namespace pw::bytes
