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

#include <limits>

#include "pw_flatbuffers_test_schema/simple_test_generated.h"
#include "pw_unit_test/framework.h"

namespace pw::flatbuffers {
namespace {

TEST(ZeroInit, SimpleTest) {
  ScalarTypes s = ScalarTypes();

  // Constructor should set all to zero
  EXPECT_EQ(s.b8(), 0);
  EXPECT_EQ(s.u8(), 0U);
  EXPECT_EQ(s.i16(), 0);
  EXPECT_EQ(s.u16(), 0U);
  EXPECT_EQ(s.i32(), 0);
  EXPECT_EQ(s.u32(), 0U);
  EXPECT_EQ(s.i64(), 0);
  EXPECT_EQ(s.u64(), 0U);
  EXPECT_FLOAT_EQ(s.f32(), 0.0f);
  EXPECT_DOUBLE_EQ(s.f64(), 0.0);
}

TEST(MaxInit, SimpleTest) {
  ScalarTypes s = ScalarTypes(std::numeric_limits<int8_t>::max(),
                              std::numeric_limits<uint8_t>::max(),
                              std::numeric_limits<int16_t>::max(),
                              std::numeric_limits<uint16_t>::max(),
                              std::numeric_limits<int32_t>::max(),
                              std::numeric_limits<uint32_t>::max(),
                              std::numeric_limits<int64_t>::max(),
                              std::numeric_limits<uint64_t>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<double>::max());

  // Constructor should set all to max
  EXPECT_EQ(s.b8(), std::numeric_limits<int8_t>::max());
  EXPECT_EQ(s.u8(), std::numeric_limits<uint8_t>::max());
  EXPECT_EQ(s.i16(), std::numeric_limits<int16_t>::max());
  EXPECT_EQ(s.u16(), std::numeric_limits<uint16_t>::max());
  EXPECT_EQ(s.i32(), std::numeric_limits<int32_t>::max());
  EXPECT_EQ(s.u32(), std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(s.i64(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(s.u64(), std::numeric_limits<uint64_t>::max());
  EXPECT_FLOAT_EQ(s.f32(), std::numeric_limits<float>::max());
  EXPECT_DOUBLE_EQ(s.f64(), std::numeric_limits<double>::max());
}

TEST(MinInit, SimpleTest) {
  ScalarTypes s = ScalarTypes(std::numeric_limits<int8_t>::min(),
                              std::numeric_limits<uint8_t>::min(),
                              std::numeric_limits<int16_t>::min(),
                              std::numeric_limits<uint16_t>::min(),
                              std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<uint32_t>::min(),
                              std::numeric_limits<int64_t>::min(),
                              std::numeric_limits<uint64_t>::min(),
                              std::numeric_limits<float>::min(),
                              std::numeric_limits<double>::min());

  // Constructor should set all to max
  EXPECT_EQ(s.b8(), std::numeric_limits<int8_t>::min());
  EXPECT_EQ(s.u8(), std::numeric_limits<uint8_t>::min());
  EXPECT_EQ(s.i16(), std::numeric_limits<int16_t>::min());
  EXPECT_EQ(s.u16(), std::numeric_limits<uint16_t>::min());
  EXPECT_EQ(s.i32(), std::numeric_limits<int32_t>::min());
  EXPECT_EQ(s.u32(), std::numeric_limits<uint32_t>::min());
  EXPECT_EQ(s.i64(), std::numeric_limits<int64_t>::min());
  EXPECT_EQ(s.u64(), std::numeric_limits<uint64_t>::min());
  EXPECT_FLOAT_EQ(s.f32(), std::numeric_limits<float>::min());
  EXPECT_DOUBLE_EQ(s.f64(), std::numeric_limits<double>::min());
}

}  // namespace
}  // namespace pw::flatbuffers
