// Copyright 2023 The Pigweed Authors
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

#include "pw_async2/poll.h"

#include <type_traits>

#include "gtest/gtest.h"
#include "pw_result/result.h"

namespace pw::async2 {
namespace {

class MoveOnly {
 public:
  MoveOnly() = delete;
  MoveOnly(int value) : value_(value) {}
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;
  int value() const { return value_; }

 private:
  int value_;
};

struct Immovable {
 public:
  Immovable() = delete;
  Immovable(MoveOnly&& value) : value_(std::move(value)) {}
  Immovable(const Immovable&) = delete;
  Immovable& operator=(const Immovable&) = delete;
  Immovable(Immovable&&) = delete;
  Immovable& operator=(Immovable&&) = delete;
  int value() const { return value_.value(); }

 private:
  MoveOnly value_;
};

TEST(Poll, ConstructsReadyInPlace) {
  Poll<Immovable> mr(std::in_place, MoveOnly(5));
  EXPECT_TRUE(mr.IsReady());
  EXPECT_EQ(mr->value(), 5);
}

TEST(Poll, ConstructsReadyFromValueType) {
  Poll<MoveOnly> mr = MoveOnly(5);
  EXPECT_TRUE(mr.IsReady());
  EXPECT_EQ(mr->value(), 5);
}

TEST(Poll, ConstructsFromValueConvertibleToValueType) {
  Poll<Immovable> mr(MoveOnly(5));
  EXPECT_TRUE(mr.IsReady());
  EXPECT_EQ(mr->value(), 5);
}

TEST(Poll, ConstructsFromPollWithValueConvertibleToValueType) {
  Poll<MoveOnly> move_poll(MoveOnly(5));
  Poll<Immovable> no_move_poll(std::move(move_poll));
  EXPECT_TRUE(no_move_poll.IsReady());
  EXPECT_EQ(no_move_poll->value(), 5);
}

TEST(Poll, ConstructsPendingFromPendingType) {
  Poll<MoveOnly> mr(Pending());
  EXPECT_FALSE(mr.IsReady());
}

TEST(Poll, ConstructorInfersValueType) {
  auto res = Poll("hello");
  static_assert(std::is_same_v<decltype(res), Poll<const char*>>);
  EXPECT_TRUE(res.IsReady());
  EXPECT_STREQ(res.value(), "hello");
}

TEST(PendingFunction, ReturnsValueConvertibleToPendingPoll) {
  Poll<MoveOnly> mr = Pending();
  EXPECT_FALSE(mr.IsReady());
}

TEST(ReadyFunction, CalledWithNoArgumentsReturnsPollWithReadyType) {
  Poll<> mr = Ready();
  EXPECT_TRUE(mr.IsReady());
  [[maybe_unused]] ReadyType& ready_value = mr.value();
}

TEST(ReadyFunction, ConstructsReadyInPlace) {
  Poll<Immovable> mr = Ready<Immovable>(std::in_place, MoveOnly(5));
  EXPECT_TRUE(mr.IsReady());
  EXPECT_EQ(mr->value(), 5);
}

TEST(ReadyFunction, ConstructsReadyFromValueType) {
  Poll<MoveOnly> mr = Ready(MoveOnly(5));
  EXPECT_TRUE(mr.IsReady());
  EXPECT_EQ(mr->value(), 5);
}

Poll<Result<int>> EndToEndTest(int input) {
  if (input == 0) {
    // Check that returning plain ``Status`` works.
    return Status::PermissionDenied();
  }
  if (input == 1) {
    // Check that returning ``Pending`` works.
    return Pending();
  }
  if (input == 2) {
    // Check that returning ``Result<int>`` works.
    Result<int> v = 2;
    return v;
  }
  if (input == 3) {
    // Check that returning plain ``int`` works.
    return 3;
  }
  if (input == 4) {
    // Check that returning ``Poll<int>`` works.
    return Ready(4);
  }
  if (input == 5) {
    // Check that returning ``Poll<Status>`` works.
    return Ready(Status::DataLoss());
  }
  return Status::Unknown();
}

TEST(EndToEndTest, ReturnsStatus) {
  Poll<Result<int>> result = EndToEndTest(0);
  ASSERT_TRUE(result.IsReady());
  EXPECT_EQ(result->status(), Status::PermissionDenied());
}

TEST(EndToEndTest, ReturnsPending) {
  Poll<Result<int>> result = EndToEndTest(1);
  EXPECT_FALSE(result.IsReady());
}

TEST(EndToEndTest, ReturnsValue) {
  Poll<Result<int>> result = EndToEndTest(3);
  ASSERT_TRUE(result.IsReady());
  ASSERT_TRUE(result->ok());
  EXPECT_EQ(**result, 3);
}

TEST(EndToEndTest, ReturnsReady) {
  Poll<Result<int>> result = EndToEndTest(4);
  ASSERT_TRUE(result.IsReady());
  ASSERT_TRUE(result->ok());
  EXPECT_EQ(**result, 4);
}

TEST(EndToEndTest, ReturnsPollStatus) {
  Poll<Result<int>> result = EndToEndTest(5);
  ASSERT_TRUE(result.IsReady());
  EXPECT_EQ(result->status(), Status::DataLoss());
}

}  // namespace
}  // namespace pw::async2
