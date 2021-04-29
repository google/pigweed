// Copyright 2021 The Pigweed Authors
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

#include "pw_string/util.h"

#include "gtest/gtest.h"

namespace pw::string {
namespace {

TEST(Length, Nullptr_Returns0) { EXPECT_EQ(0u, Length(nullptr, 100)); }

TEST(Length, EmptyString_Returns0) {
  EXPECT_EQ(0u, Length("", 0));
  EXPECT_EQ(0u, Length("", 100));
}

TEST(Length, MaxLongerThanString_ReturnsStrlen) {
  EXPECT_EQ(5u, Length("12345", 100));
}

TEST(Length, StringMaxLongerThanMax_ReturnsMax) {
  EXPECT_EQ(0u, Length("12345", 0));
  EXPECT_EQ(4u, Length("12345", 4));
}

TEST(Length, LengthEqualsMax) { EXPECT_EQ(5u, Length("12345", 5)); }

class TestWithBuffer : public ::testing::Test {
 protected:
  static constexpr char kStartingString[] = "!@#$%^&*()!@#$%^&*()";

  TestWithBuffer() { std::memcpy(buffer_, kStartingString, sizeof(buffer_)); }

  char buffer_[sizeof(kStartingString)];
};

class CopyTest : public TestWithBuffer {};

using namespace std::literals::string_view_literals;

TEST_F(CopyTest, EmptyStringView_WritesNullTerminator) {
  EXPECT_EQ(0u, Copy("", buffer_).size());
  EXPECT_EQ('\0', buffer_[0]);
}

TEST_F(CopyTest, EmptyBuffer_WritesNothing) {
  auto result = Copy("Hello", std::span(buffer_, 0));
  EXPECT_EQ(0u, result.size());
  EXPECT_FALSE(result.ok());
  EXPECT_STREQ(kStartingString, buffer_);
}

TEST_F(CopyTest, TooSmall_Truncates) {
  auto result = Copy("Hi!", std::span(buffer_, 3));
  EXPECT_EQ(2u, result.size());
  EXPECT_FALSE(result.ok());
  EXPECT_STREQ("Hi", buffer_);
}

TEST_F(CopyTest, ExactFit) {
  auto result = Copy("Hi!", std::span(buffer_, 4));
  EXPECT_EQ(3u, result.size());
  EXPECT_TRUE(result.ok());
  EXPECT_STREQ("Hi!", buffer_);
}

TEST_F(CopyTest, NullTerminatorsInString) {
  ASSERT_EQ(4u, Copy("\0!\0\0"sv, std::span(buffer_, 5)).size());
  EXPECT_EQ("\0!\0\0"sv, std::string_view(buffer_, 4));
}

}  // namespace
}  // namespace pw::string
