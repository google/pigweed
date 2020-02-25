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

#include "pw_status/status_with_size.h"

#include "gtest/gtest.h"

namespace pw {
namespace {

static_assert(StatusWithSize::max_size() ==
                  (static_cast<size_t>(1) << (sizeof(size_t) * 8 - 5)) - 1,
              "max_size() should use all but the top 5 bits of a size_t.");

TEST(StatusWithSize, Default) {
  StatusWithSize result;
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(0u, result.size());
}

TEST(StatusWithSize, ConstructWithSize) {
  StatusWithSize result = StatusWithSize(456);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(456u, result.size());
}

TEST(StatusWithSize, ConstructWithError) {
  StatusWithSize result(Status::RESOURCE_EXHAUSTED, 123);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, result.status());
  EXPECT_EQ(123u, result.size());
}

TEST(StatusWithSize, ConstructWithOkAndSize) {
  StatusWithSize result(Status::OK, 99);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(99u, result.size());
}

TEST(StatusWithSize, ConstructFromConstant) {
  StatusWithSize result(StatusWithSize::ALREADY_EXISTS);

  EXPECT_EQ(Status::ALREADY_EXISTS, result.status());
  EXPECT_EQ(0u, result.size());

  result = StatusWithSize::NOT_FOUND;

  EXPECT_EQ(Status::NOT_FOUND, result.status());
  EXPECT_EQ(0u, result.size());
}

TEST(StatusWithSize, AllStatusValues_ZeroSize) {
  for (int i = 0; i < 32; ++i) {
    StatusWithSize result(static_cast<Status::Code>(i), 0);
    EXPECT_EQ(result.ok(), i == 0);
    EXPECT_EQ(i, static_cast<int>(result.status()));
    EXPECT_EQ(0u, result.size());
  }
}

TEST(StatusWithSize, AllStatusValues_SameSize) {
  for (int i = 0; i < 32; ++i) {
    StatusWithSize result(static_cast<Status::Code>(i), i);
    EXPECT_EQ(result.ok(), i == 0);
    EXPECT_EQ(i, static_cast<int>(result.status()));
    EXPECT_EQ(static_cast<size_t>(i), result.size());
  }
}

TEST(StatusWithSize, AllStatusValues_MaxSize) {
  for (int i = 0; i < 32; ++i) {
    StatusWithSize result(static_cast<Status::Code>(i),
                          StatusWithSize::max_size());
    EXPECT_EQ(result.ok(), i == 0);
    EXPECT_EQ(i, static_cast<int>(result.status()));
    EXPECT_EQ(result.max_size(), result.size());
  }
}

TEST(StatusWithSize, Assignment) {
  StatusWithSize result = StatusWithSize(Status::INTERNAL, 0x123);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(Status::INTERNAL, result.status());
  EXPECT_EQ(0x123u, result.size());

  result = StatusWithSize(300);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(300u, result.size());
}

TEST(StatusWithSize, Constexpr) {
  constexpr StatusWithSize result(Status::CANCELLED, 1234);
  static_assert(Status::CANCELLED == result.status());
  static_assert(!result.ok());
  static_assert(1234u == result.size());
}

}  // namespace
}  // namespace pw
