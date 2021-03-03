// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pipeline_monitor.h"

#include <memory>

#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt {
namespace {

// Use a test dispatch loop.
class PipelineMonitorTest : public gtest::TestLoopFixture {};

TEST_F(PipelineMonitorTest, TokensCanOutliveMonitor) {
  auto monitor = std::make_unique<PipelineMonitor>(fit::nullable{dispatcher()});
  auto token = monitor->Issue(0);
  monitor.reset();
}

TEST_F(PipelineMonitorTest, SequentialTokensModifyCounts) {
  PipelineMonitor monitor(fit::nullable{dispatcher()});
  EXPECT_EQ(0U, monitor.bytes_issued());
  EXPECT_EQ(0, monitor.tokens_issued());
  EXPECT_EQ(0U, monitor.bytes_in_flight());
  EXPECT_EQ(0, monitor.tokens_in_flight());
  EXPECT_EQ(0U, monitor.bytes_retired());
  EXPECT_EQ(0, monitor.tokens_retired());

  constexpr size_t kByteCount = 2;
  {
    auto token = monitor.Issue(kByteCount);
    EXPECT_EQ(kByteCount, monitor.bytes_issued());
    EXPECT_EQ(1, monitor.tokens_issued());
    EXPECT_EQ(kByteCount, monitor.bytes_in_flight());
    EXPECT_EQ(1, monitor.tokens_in_flight());
    EXPECT_EQ(0U, monitor.bytes_retired());
    EXPECT_EQ(0, monitor.tokens_retired());

    token.Retire();
    EXPECT_EQ(kByteCount, monitor.bytes_issued());
    EXPECT_EQ(1, monitor.tokens_issued());
    EXPECT_EQ(0U, monitor.bytes_in_flight());
    EXPECT_EQ(0, monitor.tokens_in_flight());
    EXPECT_EQ(kByteCount, monitor.bytes_retired());
    EXPECT_EQ(1, monitor.tokens_retired());

    // Test that a moved-from value is reusable and that it retires by going out of scope
    token = monitor.Issue(kByteCount);
    EXPECT_EQ(2 * kByteCount, monitor.bytes_issued());
    EXPECT_EQ(2, monitor.tokens_issued());
    EXPECT_EQ(kByteCount, monitor.bytes_in_flight());
    EXPECT_EQ(1, monitor.tokens_in_flight());
    EXPECT_EQ(kByteCount, monitor.bytes_retired());
    EXPECT_EQ(1, monitor.tokens_retired());
  }

  EXPECT_EQ(2 * kByteCount, monitor.bytes_issued());
  EXPECT_EQ(2, monitor.tokens_issued());
  EXPECT_EQ(0U, monitor.bytes_in_flight());
  EXPECT_EQ(0, monitor.tokens_in_flight());
  EXPECT_EQ(2 * kByteCount, monitor.bytes_retired());
  EXPECT_EQ(2, monitor.tokens_retired());
}

TEST_F(PipelineMonitorTest, TokensCanBeMoved) {
  PipelineMonitor monitor(fit::nullable{dispatcher()});
  EXPECT_EQ(0U, monitor.bytes_issued());
  EXPECT_EQ(0, monitor.tokens_issued());
  EXPECT_EQ(0U, monitor.bytes_in_flight());
  EXPECT_EQ(0, monitor.tokens_in_flight());
  EXPECT_EQ(0U, monitor.bytes_retired());
  EXPECT_EQ(0, monitor.tokens_retired());

  constexpr size_t kByteCount = 2;
  auto token0 = monitor.Issue(kByteCount);
  auto token1 = std::move(token0);
  EXPECT_EQ(kByteCount, monitor.bytes_issued());
  EXPECT_EQ(1, monitor.tokens_issued());
  EXPECT_EQ(kByteCount, monitor.bytes_in_flight());
  EXPECT_EQ(1, monitor.tokens_in_flight());
  EXPECT_EQ(0U, monitor.bytes_retired());
  EXPECT_EQ(0, monitor.tokens_retired());

  // both active token and moved-from token can be retired safely
  token0.Retire();
  token1.Retire();
  EXPECT_EQ(kByteCount, monitor.bytes_issued());
  EXPECT_EQ(1, monitor.tokens_issued());
  EXPECT_EQ(0U, monitor.bytes_in_flight());
  EXPECT_EQ(0, monitor.tokens_in_flight());
  EXPECT_EQ(kByteCount, monitor.bytes_retired());
  EXPECT_EQ(1, monitor.tokens_retired());
}

}  // namespace
}  // namespace bt
