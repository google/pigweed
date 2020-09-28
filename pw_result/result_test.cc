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

#include "pw_result/result.h"

#include "gtest/gtest.h"

namespace pw {
namespace {

TEST(Result, CreateOk) {
  Result<const char*> res("hello");
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(res.status(), Status::Ok());
  EXPECT_EQ(res.value(), "hello");
}

TEST(Result, CreateNotOk) {
  Result<int> res(Status::DataLoss());
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.status(), Status::DataLoss());
}

TEST(Result, ValueOr) {
  Result<int> good(3);
  Result<int> bad(Status::DataLoss());
  EXPECT_EQ(good.value_or(42), 3);
  EXPECT_EQ(bad.value_or(42), 42);
}

TEST(Result, ConstructType) {
  struct Point {
    Point(int a, int b) : x(a), y(b) {}

    int x;
    int y;
  };

  Result<Point> origin{std::in_place, 0, 0};
  ASSERT_TRUE(origin.ok());
  ASSERT_EQ(origin.value().x, 0);
  ASSERT_EQ(origin.value().y, 0);
}

Result<float> Divide(float a, float b) {
  if (b == 0) {
    return Status::InvalidArgument();
  }
  return a / b;
}

TEST(Divide, ReturnOk) {
  Result<float> res = Divide(10, 5);
  ASSERT_TRUE(res.ok());
  EXPECT_EQ(res.value(), 2.0f);
}

TEST(Divide, ReturnNotOk) {
  Result<float> res = Divide(10, 0);
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(res.status(), Status::InvalidArgument());
}

}  // namespace
}  // namespace pw
