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

#include "pw_string/vector.h"

#include <string>
#include <string_view>

#include "gtest/gtest.h"

namespace pw::string {
namespace {

using namespace std::literals::string_view_literals;

TEST(CopyIntoVectorTest, EmptyStringView) {
  pw::Vector<char, 32> vector{};
  EXPECT_EQ(0u, Copy("", vector).size());
  EXPECT_EQ(0u, vector.size());
}

TEST(CopyIntoVectorTest, EmptyVector_WritesNothing) {
  pw::Vector<char, 0> vector{};
  auto result = Copy("Hello", vector);
  EXPECT_EQ(0u, result.size());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(0u, vector.size());
}

TEST(CopyIntoVectorTest, TooSmall_Truncates) {
  pw::Vector<char, 2> vector{};
  auto result = Copy("Hi!"sv, vector);
  EXPECT_EQ(2u, result.size());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(2u, vector.size());
  EXPECT_EQ("Hi"sv, std::string_view(vector.data(), vector.size()));
}

TEST(CopyIntoVectorTest, ExactFit) {
  pw::Vector<char, 3> vector{};
  auto result = Copy("Hi!", vector);
  EXPECT_EQ(3u, result.size());
  EXPECT_TRUE(result.ok());
  EXPECT_EQ("Hi!"sv, std::string_view(vector.data(), vector.size()));
}

TEST(CopyIntoVectorTest, NullTerminatorsInString) {
  pw::Vector<char, 32> vector{};
  ASSERT_EQ(4u, Copy("\0!\0\0"sv, vector).size());
  EXPECT_EQ("\0!\0\0"sv, std::string_view(vector.data(), vector.size()));
}

class TestWithBuffer : public ::testing::Test {
 protected:
  static constexpr char kStartingString[] = "!@#$%^&*()!@#$%^&*()";

  TestWithBuffer() { std::memcpy(buffer_, kStartingString, sizeof(buffer_)); }

  char buffer_[sizeof(kStartingString)];
};

class CopyFromVectorTest : public TestWithBuffer {};

TEST_F(CopyFromVectorTest, EmptyVector_WritesNullTerminator) {
  const pw::Vector<char, 32> vector{};
  EXPECT_EQ(0u, Copy(vector, buffer_).size());
  EXPECT_EQ('\0', buffer_[0]);
}

TEST_F(CopyFromVectorTest, EmptyBuffer_WritesNothing) {
  const pw::Vector<char, 32> vector{'H', 'e', 'l', 'l', 'o'};
  auto result = Copy(vector, std::span(buffer_, 0));
  EXPECT_EQ(0u, result.size());
  EXPECT_FALSE(result.ok());
  EXPECT_STREQ(kStartingString, buffer_);
}

TEST_F(CopyFromVectorTest, TooSmall_Truncates) {
  const pw::Vector<char, 32> vector{'H', 'i', '!'};
  auto result = Copy(vector, std::span(buffer_, 3));
  EXPECT_EQ(2u, result.size());
  EXPECT_FALSE(result.ok());
  EXPECT_STREQ("Hi", buffer_);
}

TEST_F(CopyFromVectorTest, ExactFit) {
  const pw::Vector<char, 32> vector{'H', 'i', '!'};
  auto result = Copy(vector, std::span(buffer_, 4));
  EXPECT_EQ(3u, result.size());
  EXPECT_TRUE(result.ok());
  EXPECT_STREQ("Hi!", buffer_);
}

TEST_F(CopyFromVectorTest, NullTerminatorsInString) {
  const pw::Vector<char, 32> vector{'\0', '!', '\0', '\0'};
  ASSERT_EQ(4u, Copy(vector, std::span(buffer_, 5)).size());
  EXPECT_EQ("\0!\0\0"sv, std::string_view(buffer_, 4));
}

}  // namespace
}  // namespace pw::string
