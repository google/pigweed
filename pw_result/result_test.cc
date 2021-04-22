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

// pw::Result is derived from absl::StatusOr, but has some small differences.
// This test covers basic pw::Result functionality and as well as the features
// supported by pw::Result that are not supported by absl::StatusOr (constexpr
// use in particular).
//
// The complete, thorough pw::Result tests are in statusor_test.cc, which is
// derived from Abseil's tests for absl::StatusOr.

#include "pw_result/result.h"

#include "gtest/gtest.h"
#include "pw_status/try.h"

namespace pw {
namespace {

TEST(Result, CreateOk) {
  Result<const char*> res("hello");
  EXPECT_TRUE(res.ok());
  EXPECT_EQ(res.status(), OkStatus());
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

TEST(Result, Deref) {
  struct Tester {
    constexpr bool True() { return true; };
    constexpr bool False() { return false; };
  };

  auto tester = Result<Tester>(Tester());
  EXPECT_TRUE(tester.ok());
  EXPECT_TRUE(tester->True());
  EXPECT_FALSE(tester->False());
  EXPECT_TRUE((*tester).True());
  EXPECT_FALSE((*tester).False());
  EXPECT_EQ(tester.value().True(), tester->True());
  EXPECT_EQ(tester.value().False(), tester->False());
}

TEST(Result, ConstDeref) {
  struct Tester {
    constexpr bool True() const { return true; };
    constexpr bool False() const { return false; };
  };

  const auto tester = Result<Tester>(Tester());
  EXPECT_TRUE(tester.ok());
  EXPECT_TRUE(tester->True());
  EXPECT_FALSE(tester->False());
  EXPECT_TRUE((*tester).True());
  EXPECT_FALSE((*tester).False());
  EXPECT_EQ(tester.value().True(), tester->True());
  EXPECT_EQ(tester.value().False(), tester->False());
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

Result<bool> ReturnResult(Result<bool> result) { return result; }

Status TryResultAssign(Result<bool> result) {
  PW_TRY_ASSIGN(const bool value, ReturnResult(result));

  // Any status other than OK should have already returned.
  EXPECT_EQ(result.status(), OkStatus());
  EXPECT_EQ(value, result.value());
  return result.status();
}

// TODO(pwbug/363): Once pw::Result has been refactored to properly support
// non-default move and/or copy assignment operators and/or constructors, we
// should add explicit tests to confirm this is properly handled by
// PW_TRY_ASSIGN.
TEST(Result, TryAssign) {
  EXPECT_EQ(TryResultAssign(Status::Cancelled()), Status::Cancelled());
  EXPECT_EQ(TryResultAssign(Status::DataLoss()), Status::DataLoss());
  EXPECT_EQ(TryResultAssign(Status::Unimplemented()), Status::Unimplemented());
  EXPECT_EQ(TryResultAssign(false), OkStatus());
  EXPECT_EQ(TryResultAssign(true), OkStatus());
}

struct Value {
  int number;
};

TEST(Result, ConstexprOk) {
  static constexpr pw::Result<Value> kResult(Value{123});

  static_assert(kResult.status() == pw::OkStatus());
  static_assert(kResult.ok());

  static_assert((*kResult).number == 123);
  static_assert((*std::move(kResult)).number == 123);

  static_assert(kResult->number == 123);
  static_assert(std::move(kResult)->number == 123);

  static_assert(kResult.value().number == 123);
  static_assert(std::move(kResult).value().number == 123);

  static_assert(kResult.value_or(Value{99}).number == 123);
  static_assert(std::move(kResult).value_or(Value{99}).number == 123);
}

TEST(Result, ConstexprNotOk) {
  static constexpr pw::Result<Value> kResult(pw::Status::NotFound());

  static_assert(kResult.status() == pw::Status::NotFound());
  static_assert(!kResult.ok());

  static_assert(kResult.value_or(Value{99}).number == 99);
  static_assert(std::move(kResult).value_or(Value{99}).number == 99);
}

TEST(Result, ConstexprNotOkCopy) {
  static constexpr pw::Result<Value> kResult(pw::Status::NotFound());
  constexpr pw::Result<Value> kResultCopy(kResult);

  static_assert(kResultCopy.status() == pw::Status::NotFound());
  static_assert(!kResultCopy.ok());

  static_assert(kResultCopy.value_or(Value{99}).number == 99);
  static_assert(std::move(kResultCopy).value_or(Value{99}).number == 99);
}

}  // namespace
}  // namespace pw
