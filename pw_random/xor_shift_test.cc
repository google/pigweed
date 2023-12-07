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
#include "pw_random/xor_shift.h"

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "pw_assert/config.h"
#include "pw_unit_test/framework.h"

namespace pw::random {
namespace {

constexpr uint64_t seed1 = 5;
constexpr uint64_t result1[] = {
    0x423212e85fb37474u,
    0x96051f25a1aadc74u,
    0x8ac1f520f5595a79u,
    0x7587fe57095b7c11u,
};
constexpr int result1_count = sizeof(result1) / sizeof(result1[0]);

constexpr uint64_t seed2 = 0x21feabcd5fb37474u;
constexpr uint64_t result2[] = {
    0x568ea260a4f3e793u,
    0x5ea87d669ab04d36u,
    0x77a8675eec48ae8bu,
};
constexpr int result2_count = sizeof(result2) / sizeof(result2[0]);

TEST(XorShiftStarRng64, ValidateSeries1) {
  XorShiftStarRng64 rng(seed1);
  for (size_t i = 0; i < result1_count; ++i) {
    uint64_t val = 0;
    rng.GetInt(val);
    EXPECT_EQ(val, result1[i]);
  }
}

TEST(XorShiftStarRng64, ValidateSeries2) {
  XorShiftStarRng64 rng(seed2);
  for (size_t i = 0; i < result2_count; ++i) {
    uint64_t val = 0;
    rng.GetInt(val);
    EXPECT_EQ(val, result2[i]);
  }
}

TEST(XorShiftStarRng64, InjectEntropyBits) {
  XorShiftStarRng64 rng(seed1);
  uint64_t val = 0;
  rng.InjectEntropyBits(0x1, 1);
  rng.GetInt(val);
  EXPECT_NE(val, result1[0]);
}

TEST(XorShiftStarRng64, Inject32BitsEntropy) {
  XorShiftStarRng64 rng_1(seed1);
  uint64_t first_val = 0;
  rng_1.InjectEntropyBits(0x12345678, 32);
  rng_1.GetInt(first_val);
  EXPECT_NE(first_val, result1[0]);
}

// Ensure injecting the same entropy integer, but different bit counts causes
// the randomly generated number to differ.
TEST(XorShiftStarRng64, EntropyBitCount) {
  XorShiftStarRng64 rng_1(seed1);
  uint64_t first_val = 0;
  rng_1.InjectEntropyBits(0x1, 1);
  rng_1.GetInt(first_val);

  // Use the same starting seed.
  XorShiftStarRng64 rng_2(seed1);
  uint64_t second_val = 0;
  // Use a different number of entropy bits.
  rng_2.InjectEntropyBits(0x1, 2);
  rng_2.GetInt(second_val);

  EXPECT_NE(first_val, second_val);
}

// Ensure injecting the same integer bit-by-bit applies the same transformation
// as all in one call. This lets applications decide which is more convenient
// without worrying about algorithmic changes.
TEST(XorShiftStarRng64, IncrementalEntropy) {
  XorShiftStarRng64 rng_1(seed1);
  uint64_t first_val = 0;
  rng_1.InjectEntropyBits(0x6, 3);
  rng_1.GetInt(first_val);

  // Use the same starting seed.
  XorShiftStarRng64 rng_2(seed1);
  uint64_t second_val = 0;
  // Use a different number of injection calls. 6 = 0b110
  rng_2.InjectEntropyBits(0x1, 1);
  rng_2.InjectEntropyBits(0x1, 1);
  rng_2.InjectEntropyBits(0x0, 1);
  rng_2.GetInt(second_val);

  EXPECT_EQ(first_val, second_val);
}

TEST(XorShiftStarRng64, InjectEntropy) {
  XorShiftStarRng64 rng(seed1);
  uint64_t val = 0;
  constexpr std::array<const std::byte, 5> entropy{std::byte(0xaf),
                                                   std::byte(0x9b),
                                                   std::byte(0x33),
                                                   std::byte(0x17),
                                                   std::byte(0x02)};
  rng.InjectEntropy(entropy);
  rng.GetInt(val);
  EXPECT_NE(val, result1[0]);
}

TEST(XorShiftStarRng64, GetIntBoundedUint8) {
  XorShiftStarRng64 rng(seed1);

  constexpr uint8_t upper_bound = 150;

  constexpr uint8_t result[] = {
      116,
      116,
      121,
      17,
      46,
      137,
      121,
      114,
      44,
  };
  constexpr int result_count = sizeof(result) / sizeof(result[0]);

  uint8_t val8 = 0;
  for (int i = 0; i < result_count; i++) {
    rng.GetInt(val8, upper_bound);
    EXPECT_EQ(val8, result[i]);
  }
}

TEST(XorShiftStarRng64, GetIntBoundedUint16) {
  XorShiftStarRng64 rng(seed1);

  constexpr uint16_t upper_bound = 400;

  constexpr uint16_t result[] = {
      116,
      116,
      121,
      17,
      302,
      137,
      121,
      370,
      300,
  };
  constexpr int result_count = sizeof(result) / sizeof(result[0]);

  uint16_t val16 = 0;
  for (int i = 0; i < result_count; i++) {
    rng.GetInt(val16, upper_bound);
    EXPECT_EQ(val16, result[i]);
  }
}

TEST(XorShiftStarRng64, GetIntBoundedUint32) {
  XorShiftStarRng64 rng(seed1);

  constexpr uint32_t upper_bound = 3'000'000'000;

  constexpr uint32_t result[] = {
      1'605'596'276,
      2'712'329'332,
      156'990'481,
      2'474'818'862,
      1'767'009'929,
      1'239'843'961,
      2'490'623'346,
  };
  constexpr int result_count = sizeof(result) / sizeof(result[0]);

  uint32_t val32 = 0;
  for (int i = 0; i < result_count; i++) {
    rng.GetInt(val32, upper_bound);
    EXPECT_EQ(val32, result[i]);
  }
}

TEST(XorShiftStarRng64, GetIntBoundedUint64) {
  XorShiftStarRng64 rng(seed1);

  constexpr uint64_t upper_bound = 10'000'000'000;

  constexpr uint64_t result[] = {
      1'605'596'276,
      7'007'296'628,
      4'116'273'785,
      6'061'977'225,
      1'239'843'961,
      6'785'590'642,
      4'181'236'647,
  };
  constexpr int result_count = sizeof(result) / sizeof(result[0]);

  uint64_t val64 = 0;
  for (int i = 0; i < result_count; i++) {
    rng.GetInt(val64, upper_bound);
    EXPECT_EQ(val64, result[i]);
  }
}

TEST(XorShiftStarRng64, GetIntBoundedAt0) {
  if (!PW_ASSERT_ENABLE_DEBUG) {
    XorShiftStarRng64 rng(seed1);
    uint64_t val64 = 0;
    rng.GetInt(val64, static_cast<uint64_t>(0));
    EXPECT_EQ(val64, 0u);
  }
}

TEST(XorShiftStarRng64, GetIntBoundedWith1IsAlways0) {
  XorShiftStarRng64 rng(seed1);
  uint64_t val64 = 0;
  for (int i = 0; i < 100; ++i) {
    rng.GetInt(val64, static_cast<uint64_t>(1));
    EXPECT_EQ(val64, 0u);
  }
}

TEST(XorShiftStarRng64, GetIntBoundedWithBoundOf2MightBeOneOrZero) {
  XorShiftStarRng64 rng(seed1);
  bool values[] = {false, false, false};
  for (int i = 0; i < 250; ++i) {
    size_t values_index = 0;
    rng.GetInt(values_index, static_cast<size_t>(2));
    values[values_index] |= true;
  }

  EXPECT_TRUE(values[0]);
  EXPECT_TRUE(values[1]);
  EXPECT_FALSE(values[2]);
}

TEST(XorShiftStarRng64, GetIntBoundedIsLowerThanPowersOfTwo) {
  XorShiftStarRng64 rng(seed1);
  for (uint64_t pow_of_2 = 0; pow_of_2 < 64; ++pow_of_2) {
    uint64_t upper_bound = static_cast<uint64_t>(1) << pow_of_2;
    uint64_t value = 0;
    for (int i = 0; i < 256; ++i) {
      rng.GetInt(value, upper_bound);
      EXPECT_LT(value, upper_bound);
    }
  }
}

TEST(XorShiftStarRng64, GetIntBoundedUint64IsLowerThanSomeNumbers) {
  XorShiftStarRng64 rng(seed1);
  uint64_t bounds[] = {7, 13, 51, 233, 181, 1025, 50323, 546778};
  size_t bounds_size = sizeof(bounds) / sizeof(bounds[0]);

  for (size_t i = 0; i < bounds_size; ++i) {
    for (int j = 0; j < 256; ++j) {
      uint64_t value = 0;
      rng.GetInt(value, bounds[i]);
      EXPECT_LT(value, bounds[i]);
    }
  }
}

TEST(XorShiftStarRng64, GetIntBoundedHasHighBitSetSometimes) {
  XorShiftStarRng64 rng(seed1);
  bool high_bit = false;

  for (int i = 0; i < 256; ++i) {
    uint64_t value = 0;
    rng.GetInt(value, std::numeric_limits<uint64_t>::max());
    high_bit |= value & (1ULL << 63);
  }

  EXPECT_TRUE(high_bit);
}

}  // namespace
}  // namespace pw::random
