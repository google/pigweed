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

#include "pw_unit_test/constexpr.h"
#include "pw_unit_test/framework.h"

namespace {

PW_CONSTEXPR_TEST(MulSatInt8, NoSaturation, {
  PW_TEST_EXPECT_EQ(int8_t{50}, pw::mul_sat<int8_t>(int8_t{10}, int8_t{5}));
  PW_TEST_EXPECT_EQ(int8_t{-50}, pw::mul_sat<int8_t>(int8_t{10}, int8_t{-5}));
  PW_TEST_EXPECT_EQ(int8_t{50}, pw::mul_sat<int8_t>(int8_t{-10}, int8_t{-5}));

  PW_TEST_EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(int8_t{0}, INT8_MAX));
  PW_TEST_EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(INT8_MAX, int8_t{0}));
  PW_TEST_EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(int8_t{0}, INT8_MIN));
  PW_TEST_EXPECT_EQ(int8_t{0}, pw::mul_sat<int8_t>(INT8_MIN, int8_t{0}));

  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MAX, int8_t{1}));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MIN, int8_t{1}));

  PW_TEST_EXPECT_EQ(int8_t{-INT8_MAX},
                    pw::mul_sat<int8_t>(INT8_MAX, int8_t{-1}));

  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MIN, int8_t{-1}));

  PW_TEST_EXPECT_EQ(int8_t{121}, pw::mul_sat<int8_t>(int8_t{11}, int8_t{11}));
  PW_TEST_EXPECT_EQ(int8_t{(INT8_MAX / 2) * 2},
                    pw::mul_sat<int8_t>(int8_t{INT8_MAX / 2}, int8_t{2}));
});

PW_CONSTEXPR_TEST(MulSatInt8, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(int8_t{11}, int8_t{12}));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(int8_t{16}, int8_t{8}));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MAX, int8_t{2}));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(int8_t{2}, INT8_MAX));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MAX, INT8_MAX));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MIN, int8_t{-2}));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::mul_sat<int8_t>(INT8_MIN, INT8_MIN));
  PW_TEST_EXPECT_EQ(INT8_MAX,
                    pw::mul_sat<int8_t>(int8_t{INT8_MAX / 2 + 1}, int8_t{2}));
});

PW_CONSTEXPR_TEST(MulSatInt8, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{11}, int8_t{-12}));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{-11}, int8_t{12}));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MAX, int8_t{-2}));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{-2}, INT8_MAX));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MIN, int8_t{2}));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(int8_t{2}, INT8_MIN));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::mul_sat<int8_t>(INT8_MAX, INT8_MIN));
});

PW_CONSTEXPR_TEST(MulSatUint8, NoSaturation, {
  PW_TEST_EXPECT_EQ(uint8_t{50u},
                    pw::mul_sat<uint8_t>(uint8_t{10u}, uint8_t{5u}));
  PW_TEST_EXPECT_EQ(uint8_t{0u}, pw::mul_sat<uint8_t>(uint8_t{0u}, UINT8_MAX));
  PW_TEST_EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(UINT8_MAX, uint8_t{1u}));
  PW_TEST_EXPECT_EQ(uint8_t{225u},
                    pw::mul_sat<uint8_t>(uint8_t{15u}, uint8_t{15u}));
  PW_TEST_EXPECT_EQ(uint8_t{(UINT8_MAX / 2u) * 2u},
                    pw::mul_sat<uint8_t>(uint8_t{UINT8_MAX / 2u}, uint8_t{2u}));
});

PW_CONSTEXPR_TEST(MulSatUint8, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT8_MAX,
                    pw::mul_sat<uint8_t>(uint8_t{16u}, uint8_t{16u}));
  PW_TEST_EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(UINT8_MAX, uint8_t{2u}));
  PW_TEST_EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(uint8_t{2u}, UINT8_MAX));
  PW_TEST_EXPECT_EQ(UINT8_MAX, pw::mul_sat<uint8_t>(UINT8_MAX, UINT8_MAX));
  PW_TEST_EXPECT_EQ(
      UINT8_MAX,
      pw::mul_sat<uint8_t>(uint8_t{UINT8_MAX / 2u + 1u}, uint8_t{2u}));
});

PW_CONSTEXPR_TEST(MulSatInt16, NoSaturation, {
  PW_TEST_EXPECT_EQ(int16_t{10000},
                    pw::mul_sat<int16_t>(int16_t{100}, int16_t{100}));
  PW_TEST_EXPECT_EQ(int16_t{0}, pw::mul_sat<int16_t>(int16_t{0}, INT16_MAX));
  PW_TEST_EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MAX, int16_t{1}));
  PW_TEST_EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MIN, int16_t{1}));
  PW_TEST_EXPECT_EQ(int16_t{-INT16_MAX},
                    pw::mul_sat<int16_t>(INT16_MAX, int16_t{-1}));

  PW_TEST_EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MIN, int16_t{-1}));
  PW_TEST_EXPECT_EQ(int16_t{32400},
                    pw::mul_sat<int16_t>(int16_t{180}, int16_t{180}));
  PW_TEST_EXPECT_EQ(int16_t{(INT16_MAX / 2) * 2},
                    pw::mul_sat<int16_t>(int16_t{INT16_MAX / 2}, int16_t{2}));
});

PW_CONSTEXPR_TEST(MulSatInt16, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT16_MAX,
                    pw::mul_sat<int16_t>(int16_t{180}, int16_t{183}));
  PW_TEST_EXPECT_EQ(INT16_MAX,
                    pw::mul_sat<int16_t>(int16_t{200}, int16_t{200}));
  PW_TEST_EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MAX, int16_t{2}));
  PW_TEST_EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MIN, int16_t{-2}));
  PW_TEST_EXPECT_EQ(INT16_MAX, pw::mul_sat<int16_t>(INT16_MIN, INT16_MIN));
});

PW_CONSTEXPR_TEST(MulSatInt16, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT16_MIN,
                    pw::mul_sat<int16_t>(int16_t{180}, int16_t{-183}));
  PW_TEST_EXPECT_EQ(INT16_MIN,
                    pw::mul_sat<int16_t>(int16_t{-200}, int16_t{200}));
  PW_TEST_EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MAX, int16_t{-2}));
  PW_TEST_EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MIN, int16_t{2}));
  PW_TEST_EXPECT_EQ(INT16_MIN, pw::mul_sat<int16_t>(INT16_MAX, INT16_MIN));
});

PW_CONSTEXPR_TEST(MulSatUint16, NoSaturation, {
  PW_TEST_EXPECT_EQ(uint16_t{40000u},
                    pw::mul_sat<uint16_t>(uint16_t{200u}, uint16_t{200u}));
  PW_TEST_EXPECT_EQ(uint16_t{0u},
                    pw::mul_sat<uint16_t>(uint16_t{0u}, UINT16_MAX));
  PW_TEST_EXPECT_EQ(UINT16_MAX,
                    pw::mul_sat<uint16_t>(UINT16_MAX, uint16_t{1u}));
  PW_TEST_EXPECT_EQ(uint16_t{65025u},
                    pw::mul_sat<uint16_t>(uint16_t{255u}, uint16_t{255u}));
  PW_TEST_EXPECT_EQ(
      uint16_t{(UINT16_MAX / 2u) * 2u},
      pw::mul_sat<uint16_t>(uint16_t{UINT16_MAX / 2u}, uint16_t{2u}));
});

PW_CONSTEXPR_TEST(MulSatUint16, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT16_MAX,
                    pw::mul_sat<uint16_t>(uint16_t{256u}, uint16_t{256u}));
  PW_TEST_EXPECT_EQ(UINT16_MAX,
                    pw::mul_sat<uint16_t>(UINT16_MAX, uint16_t{2u}));
  PW_TEST_EXPECT_EQ(
      UINT16_MAX,
      pw::mul_sat<uint16_t>(uint16_t{UINT16_MAX / 2u + 1u}, uint16_t{2u}));
});

PW_CONSTEXPR_TEST(MulSatInt32, NoSaturation, {
  PW_TEST_EXPECT_EQ(INT32_C(0), pw::mul_sat<int32_t>(INT32_C(0), INT32_MAX));
  PW_TEST_EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(1)));
  PW_TEST_EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(1)));
  PW_TEST_EXPECT_EQ(-INT32_MAX, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(-1)));

  PW_TEST_EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(-1)));
  PW_TEST_EXPECT_EQ(INT32_C(2147395600),
                    pw::mul_sat<int32_t>(INT32_C(46340), INT32_C(46340)));
  PW_TEST_EXPECT_EQ((INT32_MAX / 2) * 2,
                    pw::mul_sat<int32_t>(INT32_MAX / 2, INT32_C(2)));
});

PW_CONSTEXPR_TEST(MulSatInt32, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT32_MAX,
                    pw::mul_sat<int32_t>(INT32_C(46341), INT32_C(46341)));
  PW_TEST_EXPECT_EQ(INT32_MAX,
                    pw::mul_sat<int32_t>(INT32_C(65536), INT32_C(32768)));
  PW_TEST_EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(2)));
  PW_TEST_EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(-2)));
  PW_TEST_EXPECT_EQ(INT32_MAX, pw::mul_sat<int32_t>(INT32_MIN, INT32_MIN));
});

PW_CONSTEXPR_TEST(MulSatInt32, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT32_MIN,
                    pw::mul_sat<int32_t>(INT32_C(46341), INT32_C(-46341)));
  PW_TEST_EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MAX, INT32_C(-2)));
  PW_TEST_EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MIN, INT32_C(2)));
  PW_TEST_EXPECT_EQ(INT32_MIN, pw::mul_sat<int32_t>(INT32_MAX, INT32_MIN));
});

PW_CONSTEXPR_TEST(MulSatUint32, NoSaturation, {
  PW_TEST_EXPECT_EQ(UINT32_C(0),
                    pw::mul_sat<uint32_t>(UINT32_C(0), UINT32_MAX));
  PW_TEST_EXPECT_EQ(UINT32_MAX, pw::mul_sat<uint32_t>(UINT32_MAX, UINT32_C(1)));
  PW_TEST_EXPECT_EQ(UINT32_C(4294836225),
                    pw::mul_sat<uint32_t>(UINT32_C(65535), UINT32_C(65535)));
  PW_TEST_EXPECT_EQ((UINT32_MAX / 2U) * 2U,
                    pw::mul_sat<uint32_t>(UINT32_MAX / 2U, UINT32_C(2)));
});

PW_CONSTEXPR_TEST(MulSatUint32, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT32_MAX,
                    pw::mul_sat<uint32_t>(UINT32_C(65536), UINT32_C(65536)));
  PW_TEST_EXPECT_EQ(UINT32_MAX, pw::mul_sat<uint32_t>(UINT32_MAX, UINT32_C(2)));
  PW_TEST_EXPECT_EQ(UINT32_MAX,
                    pw::mul_sat<uint32_t>(UINT32_MAX / 2U + 1U, UINT32_C(2)));
});

PW_CONSTEXPR_TEST(MulSatInt64, NoSaturation, {
  PW_TEST_EXPECT_EQ(INT64_C(0), pw::mul_sat<int64_t>(INT64_C(0), INT64_MAX));
  PW_TEST_EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(1)));
  PW_TEST_EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(1)));
  PW_TEST_EXPECT_EQ(-INT64_MAX, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(-1)));

  PW_TEST_EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(-1)));

  PW_TEST_EXPECT_EQ(
      INT64_C(2147483647) * INT64_C(2147483647),
      pw::mul_sat<int64_t>(INT64_C(2147483647), INT64_C(2147483647)));
  PW_TEST_EXPECT_EQ((INT64_MAX / 2) * 2,
                    pw::mul_sat<int64_t>(INT64_MAX / 2, INT64_C(2)));
});

PW_CONSTEXPR_TEST(MulSatInt64, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(2)));
  PW_TEST_EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(-2)));
  PW_TEST_EXPECT_EQ(INT64_MAX, pw::mul_sat<int64_t>(INT64_MIN, INT64_MIN));
  PW_TEST_EXPECT_EQ(INT64_MAX,
                    pw::mul_sat<int64_t>(INT64_MAX / 2 + 100, INT64_C(3)));
});

PW_CONSTEXPR_TEST(MulSatInt64, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MAX, INT64_C(-2)));
  PW_TEST_EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MIN, INT64_C(2)));
  PW_TEST_EXPECT_EQ(INT64_MIN, pw::mul_sat<int64_t>(INT64_MAX, INT64_MIN));
});

PW_CONSTEXPR_TEST(MulSatUint64, NoSaturation, {
  PW_TEST_EXPECT_EQ(UINT64_C(0),
                    pw::mul_sat<uint64_t>(UINT64_C(0), UINT64_MAX));
  PW_TEST_EXPECT_EQ(UINT64_MAX, pw::mul_sat<uint64_t>(UINT64_MAX, UINT64_C(1)));

  PW_TEST_EXPECT_EQ(
      UINT64_C(4294967295) * UINT64_C(4294967295),
      pw::mul_sat<uint64_t>(UINT64_C(4294967295), UINT64_C(4294967295)));
  PW_TEST_EXPECT_EQ((UINT64_MAX / 2ULL) * 2ULL,
                    pw::mul_sat<uint64_t>(UINT64_MAX / 2ULL, UINT64_C(2)));
});

PW_CONSTEXPR_TEST(MulSatUint64, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT64_MAX, pw::mul_sat<uint64_t>(UINT64_MAX, UINT64_C(2)));
  PW_TEST_EXPECT_EQ(
      UINT64_MAX, pw::mul_sat<uint64_t>(UINT64_MAX / 2ULL + 1ULL, UINT64_C(2)));
  PW_TEST_EXPECT_EQ(UINT64_MAX, pw::mul_sat<uint64_t>(UINT64_MAX, UINT64_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt8, NoSaturation, {
  PW_TEST_EXPECT_EQ(int8_t{15}, pw::add_sat<int8_t>(int8_t{10}, int8_t{5}));
  PW_TEST_EXPECT_EQ(int8_t{5}, pw::add_sat<int8_t>(int8_t{10}, int8_t{-5}));
  PW_TEST_EXPECT_EQ(int8_t{-15}, pw::add_sat<int8_t>(int8_t{-10}, int8_t{-5}));
  PW_TEST_EXPECT_EQ(int8_t{-5}, pw::add_sat<int8_t>(int8_t{-10}, int8_t{5}));
});

PW_CONSTEXPR_TEST(AddSatInt8, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::add_sat<int8_t>(INT8_MAX, int8_t{1}));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::add_sat<int8_t>(int8_t{1}, INT8_MAX));
  PW_TEST_EXPECT_EQ(INT8_MAX, pw::add_sat<int8_t>(INT8_MAX, INT8_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt8, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::add_sat<int8_t>(INT8_MIN, int8_t{-1}));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::add_sat<int8_t>(int8_t{-1}, INT8_MIN));
  PW_TEST_EXPECT_EQ(INT8_MIN, pw::add_sat<int8_t>(INT8_MIN, INT8_MIN));
});

PW_CONSTEXPR_TEST(AddSatUint8, NoSaturation, {
  PW_TEST_EXPECT_EQ(uint8_t{15u},
                    pw::add_sat<uint8_t>(uint8_t{10u}, uint8_t{5u}));
});

PW_CONSTEXPR_TEST(AddSatUint8, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT8_MAX, pw::add_sat<uint8_t>(UINT8_MAX, uint8_t{1u}));
  PW_TEST_EXPECT_EQ(UINT8_MAX, pw::add_sat<uint8_t>(uint8_t{1u}, UINT8_MAX));
  PW_TEST_EXPECT_EQ(UINT8_MAX, pw::add_sat<uint8_t>(UINT8_MAX, UINT8_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt16, NoSaturation, {
  PW_TEST_EXPECT_EQ(int16_t{15}, pw::add_sat<int16_t>(int16_t{10}, int16_t{5}));
  PW_TEST_EXPECT_EQ(int16_t{5}, pw::add_sat<int16_t>(int16_t{10}, int16_t{-5}));
  PW_TEST_EXPECT_EQ(int16_t{-15},
                    pw::add_sat<int16_t>(int16_t{-10}, int16_t{-5}));
  PW_TEST_EXPECT_EQ(int16_t{-5},
                    pw::add_sat<int16_t>(int16_t{-10}, int16_t{5}));
});

PW_CONSTEXPR_TEST(AddSatInt16, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT16_MAX, pw::add_sat<int16_t>(INT16_MAX, int16_t{1}));
  PW_TEST_EXPECT_EQ(INT16_MAX, pw::add_sat<int16_t>(int16_t{1}, INT16_MAX));
  PW_TEST_EXPECT_EQ(INT16_MAX, pw::add_sat<int16_t>(INT16_MAX, INT16_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt16, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT16_MIN, pw::add_sat<int16_t>(INT16_MIN, int16_t{-1}));
  PW_TEST_EXPECT_EQ(INT16_MIN, pw::add_sat<int16_t>(int16_t{-1}, INT16_MIN));
  PW_TEST_EXPECT_EQ(INT16_MIN, pw::add_sat<int16_t>(INT16_MIN, INT16_MIN));
});

PW_CONSTEXPR_TEST(AddSatUint16, NoSaturation, {
  PW_TEST_EXPECT_EQ(uint16_t{15u},
                    pw::add_sat<uint16_t>(uint16_t{10u}, uint16_t{5u}));
});

PW_CONSTEXPR_TEST(AddSatUint16, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT16_MAX,
                    pw::add_sat<uint16_t>(UINT16_MAX, uint16_t{1u}));
  PW_TEST_EXPECT_EQ(UINT16_MAX,
                    pw::add_sat<uint16_t>(uint16_t{1u}, UINT16_MAX));
  PW_TEST_EXPECT_EQ(UINT16_MAX, pw::add_sat<uint16_t>(UINT16_MAX, UINT16_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt32, NoSaturation, {
  PW_TEST_EXPECT_EQ(INT32_C(15), pw::add_sat<int32_t>(INT32_C(10), INT32_C(5)));
  PW_TEST_EXPECT_EQ(INT32_C(5), pw::add_sat<int32_t>(INT32_C(10), INT32_C(-5)));
  PW_TEST_EXPECT_EQ(INT32_C(-15),
                    pw::add_sat<int32_t>(INT32_C(-10), INT32_C(-5)));
  PW_TEST_EXPECT_EQ(INT32_C(-5),
                    pw::add_sat<int32_t>(INT32_C(-10), INT32_C(5)));
});

PW_CONSTEXPR_TEST(AddSatInt32, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT32_MAX, pw::add_sat<int32_t>(INT32_MAX, INT32_C(1)));
  PW_TEST_EXPECT_EQ(INT32_MAX, pw::add_sat<int32_t>(INT32_C(1), INT32_MAX));
  PW_TEST_EXPECT_EQ(INT32_MAX, pw::add_sat<int32_t>(INT32_MAX, INT32_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt32, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT32_MIN, pw::add_sat<int32_t>(INT32_MIN, INT32_C(-1)));
  PW_TEST_EXPECT_EQ(INT32_MIN, pw::add_sat<int32_t>(INT32_C(-1), INT32_MIN));
  PW_TEST_EXPECT_EQ(INT32_MIN, pw::add_sat<int32_t>(INT32_MIN, INT32_MIN));
});

PW_CONSTEXPR_TEST(AddSatUint32, NoSaturation, {
  PW_TEST_EXPECT_EQ(UINT32_C(15),
                    pw::add_sat<uint32_t>(UINT32_C(10), UINT32_C(5)));
});

PW_CONSTEXPR_TEST(AddSatUint32, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT32_MAX, pw::add_sat<uint32_t>(UINT32_MAX, UINT32_C(1)));
  PW_TEST_EXPECT_EQ(UINT32_MAX, pw::add_sat<uint32_t>(UINT32_C(1), UINT32_MAX));
  PW_TEST_EXPECT_EQ(UINT32_MAX, pw::add_sat<uint32_t>(UINT32_MAX, UINT32_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt64, NoSaturation, {
  PW_TEST_EXPECT_EQ(INT64_C(15), pw::add_sat<int64_t>(INT64_C(10), INT64_C(5)));
  PW_TEST_EXPECT_EQ(INT64_C(5), pw::add_sat<int64_t>(INT64_C(10), INT64_C(-5)));
  PW_TEST_EXPECT_EQ(INT64_C(-15),
                    pw::add_sat<int64_t>(INT64_C(-10), INT64_C(-5)));
  PW_TEST_EXPECT_EQ(INT64_C(-5),
                    pw::add_sat<int64_t>(INT64_C(-10), INT64_C(5)));
});

PW_CONSTEXPR_TEST(AddSatInt64, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(INT64_MAX, pw::add_sat<int64_t>(INT64_MAX, INT64_C(1)));
  PW_TEST_EXPECT_EQ(INT64_MAX, pw::add_sat<int64_t>(INT64_C(1), INT64_MAX));
  PW_TEST_EXPECT_EQ(INT64_MAX, pw::add_sat<int64_t>(INT64_MAX, INT64_MAX));
});

PW_CONSTEXPR_TEST(AddSatInt64, NegativeSaturation, {
  PW_TEST_EXPECT_EQ(INT64_MIN, pw::add_sat<int64_t>(INT64_MIN, INT64_C(-1)));
  PW_TEST_EXPECT_EQ(INT64_MIN, pw::add_sat<int64_t>(INT64_C(-1), INT64_MIN));
  PW_TEST_EXPECT_EQ(INT64_MIN, pw::add_sat<int64_t>(INT64_MIN, INT64_MIN));
});

PW_CONSTEXPR_TEST(AddSatUint64, NoSaturation, {
  PW_TEST_EXPECT_EQ(UINT64_C(15),
                    pw::add_sat<uint64_t>(UINT64_C(10), UINT64_C(5)));
});

PW_CONSTEXPR_TEST(AddSatUint64, PositiveSaturation, {
  PW_TEST_EXPECT_EQ(UINT64_MAX, pw::add_sat<uint64_t>(UINT64_MAX, UINT64_C(1)));
  PW_TEST_EXPECT_EQ(UINT64_MAX, pw::add_sat<uint64_t>(UINT64_C(1), UINT64_MAX));
  PW_TEST_EXPECT_EQ(UINT64_MAX, pw::add_sat<uint64_t>(UINT64_MAX, UINT64_MAX));
});

}  // namespace
