// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pipeline_monitor.h"

#include <memory>

#include <gmock/gmock.h>

#include "src/connectivity/bluetooth/core/bt-host/common/retire_log.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt {
namespace {

// Use a test dispatch loop.
class PipelineMonitorTest : public gtest::TestLoopFixture {};

const internal::RetireLog kRetireLogDefaultParams(/*min_depth=*/1, /*max_depth=*/100);

TEST_F(PipelineMonitorTest, TokensCanOutliveMonitor) {
  auto monitor =
      std::make_unique<PipelineMonitor>(fit::nullable{dispatcher()}, kRetireLogDefaultParams);
  auto token = monitor->Issue(0);
  monitor.reset();
}

TEST_F(PipelineMonitorTest, SequentialTokensModifyCounts) {
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);
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
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);
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

TEST_F(PipelineMonitorTest, SubscribeToMaxTokensAlert) {
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);

  std::optional<PipelineMonitor::MaxTokensInFlightAlert> received_alert;
  constexpr int kMaxTokensInFlight = 1;
  monitor.SetAlert(PipelineMonitor::MaxTokensInFlightAlert{kMaxTokensInFlight},
                   [&received_alert](auto alert) { received_alert = alert; });

  // First token does not exceed in-flight threshold.
  auto token0 = monitor.Issue(0);
  EXPECT_FALSE(received_alert);

  // Total issued (but not in-flight) exceeds threshold.
  token0.Retire();
  token0 = monitor.Issue(0);
  ASSERT_LT(kMaxTokensInFlight, monitor.tokens_issued());
  EXPECT_FALSE(received_alert);

  // Total in-flight exceeds threshold.
  auto token1 = monitor.Issue(0);
  ASSERT_TRUE(received_alert.has_value());
  EXPECT_EQ(kMaxTokensInFlight + 1, received_alert.value().value);

  // Alert has expired after firing once.
  received_alert.reset();
  auto token2 = monitor.Issue(0);
  EXPECT_FALSE(received_alert);
}

TEST_F(PipelineMonitorTest, SubscribeToMaxBytesAlert) {
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);

  std::optional<PipelineMonitor::MaxBytesInFlightAlert> received_alert;
  constexpr size_t kMaxBytesInFlight = 1;
  monitor.SetAlert(PipelineMonitor::MaxBytesInFlightAlert{kMaxBytesInFlight},
                   [&received_alert](auto alert) { received_alert = alert; });

  // First token does not exceed total bytes in flight threshold.
  auto token0 = monitor.Issue(kMaxBytesInFlight);
  EXPECT_FALSE(received_alert);

  // Total in-flight exceeds threshold.
  auto token1 = monitor.Issue(1);
  ASSERT_TRUE(received_alert.has_value());
  EXPECT_EQ(kMaxBytesInFlight + 1, received_alert.value().value);
}

TEST_F(PipelineMonitorTest, SubscribeToMaxAgeAlert) {
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);

  std::optional<PipelineMonitor::MaxAgeRetiredAlert> received_alert;
  constexpr zx::duration kMaxAge = zx::msec(500);
  monitor.SetAlert(PipelineMonitor::MaxAgeRetiredAlert{kMaxAge},
                   [&received_alert](auto alert) { received_alert = alert; });

  // Token outlives threshold age, but doesn't signal alert until it's retired.
  auto token0 = monitor.Issue(0);
  RunLoopFor(kMaxAge * 2);
  EXPECT_FALSE(received_alert);

  // Total in-flight exceeds threshold.
  token0.Retire();
  ASSERT_TRUE(received_alert.has_value());
  EXPECT_EQ(kMaxAge * 2, received_alert.value().value);
}

TEST_F(PipelineMonitorTest, SubscribeToAlertInsideHandler) {
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);

  std::optional<PipelineMonitor::MaxBytesInFlightAlert> received_alert;
  constexpr size_t kMaxBytesInFlight = 2;

  auto renew_subscription = [&monitor, &received_alert](auto) {
    // Same threshold, so it should be triggered eventually, but not immediately.
    monitor.SetAlert(PipelineMonitor::MaxBytesInFlightAlert{kMaxBytesInFlight - 1},
                     [&received_alert](auto alert) { received_alert = alert; });
  };
  monitor.SetAlert(PipelineMonitor::MaxBytesInFlightAlert{kMaxBytesInFlight}, renew_subscription);

  // Total in-flight exceeds threshold.
  auto token0 = monitor.Issue(kMaxBytesInFlight + 1);
  EXPECT_FALSE(received_alert);

  // Re-subscribed alert doesn't get called until the monitored value potentially changes again.
  auto token1 = monitor.Issue(0);
  ASSERT_TRUE(received_alert.has_value());
  EXPECT_EQ(kMaxBytesInFlight + 1, received_alert.value().value);
}

TEST_F(PipelineMonitorTest, MultipleMaxBytesInFlightAlertsWithDifferentThresholds) {
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);

  std::optional<PipelineMonitor::MaxBytesInFlightAlert> received_alert_0;
  constexpr size_t kMaxBytesInFlight0 = 1;
  monitor.SetAlert(PipelineMonitor::MaxBytesInFlightAlert{kMaxBytesInFlight0},
                   [&received_alert_0](auto alert) { received_alert_0 = alert; });
  std::optional<PipelineMonitor::MaxBytesInFlightAlert> received_alert_1;
  constexpr size_t kMaxBytesInFlight1 = 2;
  monitor.SetAlert(PipelineMonitor::MaxBytesInFlightAlert{kMaxBytesInFlight1},
                   [&received_alert_1](auto alert) { received_alert_1 = alert; });

  // Total in-flight exceeds threshold 0.
  auto token0 = monitor.Issue(kMaxBytesInFlight0 + 1);
  ASSERT_TRUE(received_alert_0.has_value());
  EXPECT_LT(kMaxBytesInFlight0, received_alert_0.value().value);
  EXPECT_GE(kMaxBytesInFlight1, received_alert_0.value().value);
  EXPECT_FALSE(received_alert_1);

  // Total in-flight exceeds threshold 1.
  auto token1 = monitor.Issue(kMaxBytesInFlight1);
  ASSERT_TRUE(received_alert_1.has_value());
  EXPECT_LT(kMaxBytesInFlight1, received_alert_1.value().value);
}

TEST_F(PipelineMonitorTest, SubscribeToMultipleDissimilarAlerts) {
  PipelineMonitor monitor(fit::nullable{dispatcher()}, kRetireLogDefaultParams);

  constexpr size_t kMaxBytesInFlight = 2;
  constexpr int kMaxTokensInFlight = 1;

  int listener_call_count = 0;
  int max_bytes_alerts = 0;
  int max_tokens_alerts = 0;
  auto alerts_listener = [&](auto alert_value) {
    listener_call_count++;
    if (std::holds_alternative<PipelineMonitor::MaxBytesInFlightAlert>(alert_value)) {
      max_bytes_alerts++;
    } else if (std::holds_alternative<PipelineMonitor::MaxTokensInFlightAlert>(alert_value)) {
      max_tokens_alerts++;
    }
  };
  monitor.SetAlerts(alerts_listener, PipelineMonitor::MaxBytesInFlightAlert{kMaxBytesInFlight},
                    PipelineMonitor::MaxTokensInFlightAlert{kMaxTokensInFlight});

  auto token0 = monitor.Issue(0);
  EXPECT_EQ(0, listener_call_count);

  auto token1 = monitor.Issue(0);
  EXPECT_EQ(1, listener_call_count);
  EXPECT_EQ(1, max_tokens_alerts);

  auto token2 = monitor.Issue(kMaxBytesInFlight + 1);
  EXPECT_EQ(2, listener_call_count);
  EXPECT_EQ(1, max_bytes_alerts);
}

TEST_F(PipelineMonitorTest, TokensRetireIntoRetireLog) {
  PipelineMonitor monitor(fit::nullable{dispatcher()},
                          internal::RetireLog(/*min_depth=*/1, /*max_depth=*/64));

  auto token = monitor.Issue(1);
  EXPECT_EQ(0U, monitor.retire_log().depth());

  const zx::duration kAge = zx::msec(10);
  RunLoopFor(kAge);
  token.Retire();
  EXPECT_EQ(1U, monitor.retire_log().depth());
  const auto bytes_quantiles =
      monitor.retire_log().ComputeByteCountQuantiles(std::array{0., .5, 1.});
  ASSERT_TRUE(bytes_quantiles.has_value());
  EXPECT_THAT(*bytes_quantiles, testing::ElementsAre(1, 1, 1));

  const auto age_quantiles = monitor.retire_log().ComputeAgeQuantiles(std::array{0., .5, 1.});
  ASSERT_TRUE(age_quantiles.has_value());
  EXPECT_THAT(*age_quantiles, testing::ElementsAre(kAge, kAge, kAge));
}

}  // namespace
}  // namespace bt
