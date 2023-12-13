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

#include "gtest/gtest.h"

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

TEST(Poll, ConstructsPendingFromPendingType) {
  Poll<MoveOnly> mr(Pending());
  EXPECT_FALSE(mr.IsReady());
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

}  // namespace
}  // namespace pw::async2
