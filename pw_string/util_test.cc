// Copyright 2019 The Pigweed Authors
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

}  // namespace
}  // namespace pw::string
