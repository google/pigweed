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

#include "pw_numeric/saturating_arithmetic.h"

#include <climits>
#include <cstdint>

#include "pw_unit_test/framework.h"

namespace {

TEST(MulSatInt8, NoSaturation) {
  EXPECT_EQ(int8_t{50}, pw::mul_sat<int8_t>(int8_t{10}, int8_t{5}));
  EXPECT_EQ(int8_t{-50}, pw::mul_sat<int8_t>(int8_t{10}, int8_t{-5}));
  EXPECT_EQ(int8_t{50}, pw::mul_sat<int8_t>(int8_t{-10}, int8_t{-5}));

  EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(int8_t{0}, INT8_MAX));
  EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(INT8_MAX, int8_t{0}));
  EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(int8_t{0}, INT8_MIN));
  EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(INT8_MIN, int8_t{0}));

  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MAX, int8_t{1}));
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MIN, int8_t{1}));

  EXPECT_EQ(int8_t{-INT8_MAX}, pw::mul_sat<int8_t>(INT8_MAX, int8_t{-1}));

  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MIN, int8_t{-1}));

  EXPECT_EQ(int8_t{121}, pw::mul_sat<int8_t>(int8_t{11}, int8_t{11}));
  EXPECT_EQ(int8_t{(INT8_MAX / 2) * 2},
            pw::mul_sat<int8_t>(int8_t{INT8_MAX / 2}, int8_t{2}));
}

TEST(MulSatInt8, PositiveSaturation) {
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(int8_t{11}, int8_t{12}));
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(int8_t{16}, int8_t{8}));
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MAX, int8_t{2}));
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(int8_t{2}, INT8_MAX));
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MAX, INT8_MAX));
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MIN, int8_t{-2}));
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MIN, INT8_MIN));
  EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(int8_t{INT8_MAX / 2 + 1}, int8_t{2}));
}

TEST(MulSatInt8, NegativeSaturation) {
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{11}, int8_t{-12}));
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{-11}, int8_t{12}));
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MAX, int8_t{-2}));
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{-2}, INT8_MAX));
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MIN, int8_t{2}));
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{2}, INT8_MIN));
  EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MAX, INT8_MIN));
}

TEST(MulSatUint8, NoSaturation) {
  EXPECT_EQ(uint8_t{50u}, pw::mul_sat<uint8_t>(uint8_t{10u}, uint8_t{5u}));
  EXPECT_EQ(uint8_t{0u}, pw::mul_sat<uint8_t>(uint8_t{0u}, UINT8_MAX));
  EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(UINT8_MAX, uint8_t{1u}));
  EXPECT_EQ(uint8_t{225u}, pw::mul_sat<uint8_t>(uint8_t{15u}, uint8_t{15u}));
  EXPECT_EQ(uint8_t{(UINT8_MAX / 2u) * 2u},
            pw::mul_sat<uint8_t>(uint8_t{UINT8_MAX / 2u}, uint8_t{2u}));
}

TEST(MulSatUint8, PositiveSaturation) {
  EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(uint8_t{16u}, uint8_t{16u}));
  EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(UINT8_MAX, uint8_t{2u}));
  EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(uint8_t{2u}, UINT8_MAX));
  EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(UINT8_MAX, UINT8_MAX));
  EXPECT_EQ(UINT8_MAX,
            pw::mul_sat<uint8_t>(uint8_t{UINT8_MAX / 2u + 1u}, uint8_t{2u}));
}

TEST(MulSatInt16, NoSaturation) {
  EXPECT_EQ(int16_t{10000}, pw::mul_sat<int16_t>(int16_t{100}, int16_t{100}));
  EXPECT_EQ(int16_t{0}, pw::mul_sat<int16_t>(int16_t{0}, INT16_MAX));
  EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MAX, int16_t{1}));
  EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MIN, int16_t{1}));
  EXPECT_EQ(int16_t{-INT16_MAX}, pw::mul_sat<int16_t>(INT16_MAX, int16_t{-1}));

  EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MIN, int16_t{-1}));
  EXPECT_EQ(int16_t{32400}, pw::mul_sat<int16_t>(int16_t{180}, int16_t{180}));
  EXPECT_EQ(int16_t{(INT16_MAX / 2) * 2},
            pw::mul_sat<int16_t>(int16_t{INT16_MAX / 2}, int16_t{2}));
}

TEST(MulSatInt16, PositiveSaturation) {
  EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(int16_t{180}, int16_t{183}));
  EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(int16_t{200}, int16_t{200}));
  EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MAX, int16_t{2}));
  EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MIN, int16_t{-2}));
  EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MIN, INT16_MIN));
}

TEST(MulSatInt16, NegativeSaturation) {
  EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(int16_t{180}, int16_t{-183}));
  EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(int16_t{-200}, int16_t{200}));
  EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MAX, int16_t{-2}));
  EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MIN, int16_t{2}));
  EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MAX, INT16_MIN));
}

TEST(MulSatUint16, NoSaturation) {
  EXPECT_EQ(uint16_t{40000u},
            pw::mul_sat<uint16_t>(uint16_t{200u}, uint16_t{200u}));
  EXPECT_EQ(uint16_t{0u}, pw::mul_sat<uint16_t>(uint16_t{0u}, UINT16_MAX));
  EXPECT_EQ(UINT16_MAX, pw::mul_sat<uint16_t>(UINT16_MAX, uint16_t{1u}));
  EXPECT_EQ(uint16_t{65025u},
            pw::mul_sat<uint16_t>(uint16_t{255u}, uint16_t{255u}));
  EXPECT_EQ(uint16_t{(UINT16_MAX / 2u) * 2u},
            pw::mul_sat<uint16_t>(uint16_t{UINT16_MAX / 2u}, uint16_t{2u}));
}

TEST(MulSatUint16, PositiveSaturation) {
  EXPECT_EQ(UINT16_MAX, pw::mul_sat<uint16_t>(uint16_t{256u}, uint16_t{256u}));
  EXPECT_EQ(UINT16_MAX, pw::mul_sat<uint16_t>(UINT16_MAX, uint16_t{2u}));
  EXPECT_EQ(
      UINT16_MAX,
      pw::mul_sat<uint16_t>(uint16_t{UINT16_MAX / 2u + 1u}, uint16_t{2u}));
}

TEST(MulSatInt32, NoSaturation) {
  EXPECT_EQ(INT32_C(0), pw::mul_sat<int32_t>(INT32_C(0), INT32_MAX));
  EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(1)));
  EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(1)));
  EXPECT_EQ(-INT32_MAX, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(-1)));

  EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(-1)));
  EXPECT_EQ(INT32_C(2147395600),
            pw::mul_sat<int32_t>(INT32_C(46340), INT32_C(46340)));
  EXPECT_EQ((INT32_MAX / 2) * 2,
            pw::mul_sat<int32_t>(INT32_MAX / 2, INT32_C(2)));
}

TEST(MulSatInt32, PositiveSaturation) {
  EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_C(46341), INT32_C(46341)));
  EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_C(65536), INT32_C(32768)));
  EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(2)));
  EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(-2)));
  EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MIN, INT32_MIN));
}

TEST(MulSatInt32, NegativeSaturation) {
  EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_C(46341), INT32_C(-46341)));
  EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(-2)));
  EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(2)));
  EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MAX, INT32_MIN));
}

TEST(MulSatUint32, NoSaturation) {
  EXPECT_EQ(UINT32_C(0), pw::mul_sat<uint32_t>(UINT32_C(0), UINT32_MAX));
  EXPECT_EQ(UINT32_MAX, pw::mul_sat<uint32_t>(UINT32_MAX, UINT32_C(1)));
  EXPECT_EQ(UINT32_C(4294836225),
            pw::mul_sat<uint32_t>(UINT32_C(65535), UINT32_C(65535)));
  EXPECT_EQ((UINT32_MAX / 2U) * 2U,
            pw::mul_sat<uint32_t>(UINT32_MAX / 2U, UINT32_C(2)));
}

TEST(MulSatUint32, PositiveSaturation) {
  EXPECT_EQ(UINT32_MAX,
            pw::mul_sat<uint32_t>(UINT32_C(65536), UINT32_C(65536)));
  EXPECT_EQ(UINT32_MAX, pw::mul_sat<uint32_t>(UINT32_MAX, UINT32_C(2)));
  EXPECT_EQ(UINT32_MAX,
            pw::mul_sat<uint32_t>(UINT32_MAX / 2U + 1U, UINT32_C(2)));
}

TEST(MulSatInt64, NoSaturation) {
  EXPECT_EQ(INT64_C(0), pw::mul_sat<int64_t>(INT64_C(0), INT64_MAX));
  EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(1)));
  EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(1)));
  EXPECT_EQ(-INT64_MAX, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(-1)));

  EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(-1)));

  EXPECT_EQ(INT64_C(2147483647) * INT64_C(2147483647),
            pw::mul_sat<int64_t>(INT64_C(2147483647), INT64_C(2147483647)));
  EXPECT_EQ((INT64_MAX / 2) * 2,
            pw::mul_sat<int64_t>(INT64_MAX / 2, INT64_C(2)));
}

TEST(MulSatInt64, PositiveSaturation) {
  EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(2)));
  EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(-2)));
  EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MIN, INT64_MIN));
  EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MAX / 2 + 100, INT64_C(3)));
}

TEST(MulSatInt64, NegativeSaturation) {
  EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(-2)));
  EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(2)));
  EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MAX, INT64_MIN));
}

TEST(MulSatUint64, NoSaturation) {
  EXPECT_EQ(UINT64_C(0), pw::mul_sat<uint64_t>(UINT64_C(0), UINT64_MAX));
  EXPECT_EQ(UINT64_MAX, pw::mul_sat<uint64_t>(UINT64_MAX, UINT64_C(1)));

  EXPECT_EQ(UINT64_C(4294967295) * UINT64_C(4294967295),
            pw::mul_sat<uint64_t>(UINT64_C(4294967295), UINT64_C(4294967295)));
  EXPECT_EQ((UINT64_MAX / 2ULL) * 2ULL,
            pw::mul_sat<uint64_t>(UINT64_MAX / 2ULL, UINT64_C(2)));
}

TEST(MulSatUint64, PositiveSaturation) {
  EXPECT_EQ(UINT64_MAX, pw::mul_sat<uint64_t>(UINT64_MAX, UINT64_C(2)));
  EXPECT_EQ(UINT64_MAX,
            pw::mul_sat<uint64_t>(UINT64_MAX / 2ULL + 1ULL, UINT64_C(2)));
  EXPECT_EQ(UINT64_MAX, pw::mul_sat<uint64_t>(UINT64_MAX, UINT64_MAX));
}

}  // namespace
