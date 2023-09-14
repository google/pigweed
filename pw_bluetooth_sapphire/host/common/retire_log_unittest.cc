// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "retire_log.h"

#include <algorithm>
#include <numeric>
#include <random>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bt::internal {
namespace {

// Retire 101 entries where the fields in the range [starting_value, starting_value + 100]
void Retire101Elements(RetireLog& retire_log, int starting_value = 0) {
  std::vector<int> values(101);
  std::iota(values.begin(), values.end(), starting_value);
  constexpr int kSeed = 4;
  std::shuffle(values.begin(), values.end(), std::default_random_engine(kSeed));
  for (auto value : values) {
    retire_log.Retire(value, std::chrono::milliseconds(value));
  }
}

TEST(RetireLogDeathTest, InitializationLimits) {
  ASSERT_DEATH_IF_SUPPORTED({ RetireLog retire_log(0, 100); }, "min_depth");
  ASSERT_DEATH_IF_SUPPORTED({ RetireLog retire_log(101, 100); }, "min_depth.*max_depth");
  ASSERT_DEATH_IF_SUPPORTED({ RetireLog retire_log(1, size_t{1} << 60); }, "max_depth");
}

TEST(RetireLogDeathTest, ComputeQuantileLimits) {
  RetireLog retire_log(1, 100);
  retire_log.Retire(1, std::chrono::seconds(1));
  ASSERT_DEATH_IF_SUPPORTED(
      { [[maybe_unused]] auto _ = retire_log.ComputeByteCountQuantiles(std::array{-1.}); }, "0");
  ASSERT_DEATH_IF_SUPPORTED(
      { [[maybe_unused]] auto _ = retire_log.ComputeByteCountQuantiles(std::array{2.}); }, "1");
}

TEST(RetireLogTest, QuantileCallBeforeRetiringYieldsNothing) {
  RetireLog retire_log(1, 100);
  EXPECT_EQ(0U, retire_log.depth());
  auto result = retire_log.ComputeAgeQuantiles(std::array{.5});
  EXPECT_FALSE(result.has_value());
}

TEST(RetireLogTest, QuantileCallsAfterDepthOneYieldsTheResult) {
  RetireLog retire_log(1, 100);
  constexpr pw::chrono::SystemClock::duration kTestDuration = std::chrono::milliseconds(10);
  retire_log.Retire(0, kTestDuration);
  auto result = retire_log.ComputeAgeQuantiles(std::array{.5});
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, testing::Each(testing::Eq(kTestDuration)));
}

TEST(RetireLogTest, ComputeQuantiles) {
  RetireLog retire_log(1, 101);
  Retire101Elements(retire_log);
  auto result = retire_log.ComputeByteCountQuantiles(std::array{0., .001, .5, .754, .99, 1.});
  ASSERT_TRUE(result.has_value());

  // Cutting at extremes yields the min and max entries while cutting in the middle yields the
  // median. Cutting between entry values yields the nearest (by distribution) logged value.
  EXPECT_THAT(*result, testing::ElementsAre(0, 0, 50, 76, 99, 100));
}

TEST(RetireLogTest, ComputeQuantilesExactBoundaryIsHighBiased) {
  RetireLog retire_log(2, 2);
  retire_log.Retire(0, {});
  retire_log.Retire(1, {});
  auto result = retire_log.ComputeByteCountQuantiles(std::array{.5});
  ASSERT_TRUE(result.has_value());

  // Cutting at exactly between two entries yields the higher sample
  EXPECT_THAT(*result, testing::ElementsAre(1));
}

TEST(RetireLogTest, ComputeQuantilesOutOfOrderPartitions) {
  RetireLog retire_log(1, 101);
  Retire101Elements(retire_log);
  auto result = retire_log.ComputeByteCountQuantiles(std::array{.75, .25, .5});
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, testing::ElementsAre(75, 25, 50));
}

// Check that cutting at the same point more than once works
TEST(RetireLogTest, ComputeSameQuantileTwice) {
  RetireLog retire_log(1, 101);
  Retire101Elements(retire_log);
  auto result = retire_log.ComputeByteCountQuantiles(std::array{0., 0., 1., 1.});
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, testing::ElementsAre(0, 0, 100, 100));
}

TEST(RetireLogTest, RetiringPastMaxDepthReplacesPreviousEntries) {
  RetireLog retire_log(1, 101);
  Retire101Elements(retire_log, /*starting_value=*/0);
  Retire101Elements(retire_log, /*starting_value=*/10);
  EXPECT_EQ(101U, retire_log.depth());
  auto result = retire_log.ComputeByteCountQuantiles(std::array{0., .5, 1.});
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(*result, testing::ElementsAre(10, 60, 110));
}

}  // namespace
}  // namespace bt::internal
