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

#include "pw_metric/metric_service_nanopb.h"

#include "gtest/gtest.h"
#include "pw_log/log.h"
#include "pw_rpc/nanopb_test_method_context.h"

namespace pw::metric {
namespace {

#define MetricMethodContext      \
  PW_NANOPB_TEST_METHOD_CONTEXT( \
      MetricService, Get, 4, sizeof(pw_metric_MetricResponse))

TEST(MetricService, EmptyGroupAndNoMetrics) {
  // Empty root group.
  PW_METRIC_GROUP(root, "/");

  // Run the RPC and ensure it completes.
  MetricMethodContext context(root.metrics(), root.children());
  context.call({});
  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());

  // No metrics should be in the response.
  EXPECT_EQ(0u, context.responses().size());
}

TEST(MetricService, FlatMetricsNoGroupsOneResponseOnly) {
  // Set up a one-group suite of metrics.
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1.0);
  PW_METRIC(root, b, "b", 1.0);
  PW_METRIC(root, c, "c", 1.0);
  PW_METRIC(root, d, "d", 1.0);
  PW_METRIC(root, e, "e", 1.0);

  // Run the RPC and ensure it completes.
  MetricMethodContext context(root.metrics(), root.children());
  context.call({});
  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());

  // All of the responses should have fit in one proto.
  EXPECT_EQ(1u, context.responses().size());
  EXPECT_EQ(5, context.responses()[0].metrics_count);
}

TEST(MetricService, NestedGroupsButOnlyOneBatch) {
  // Set up a nested group of metrics that will fit in the default batch (10).
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1.0);
  PW_METRIC(root, b, "b", 1.0);
  PW_METRIC(root, c, "c", 1.0);

  PW_METRIC_GROUP(inner, "inner");
  PW_METRIC(inner, x, "x", 1.0);
  PW_METRIC(inner, y, "y", 1.0);
  PW_METRIC(inner, z, "z", 1.0);

  root.Add(inner);

  // Run the RPC and ensure it completes.
  MetricMethodContext context(root.metrics(), root.children());
  context.call({});
  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());

  // All of the responses should fit in one proto.
  EXPECT_EQ(1u, context.responses().size());
  EXPECT_EQ(6, context.responses()[0].metrics_count);
}

TEST(MetricService, NestedGroupsWithBatches) {
  // Set up a nested group of metrics that will not fit in a single batch.
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1u);
  PW_METRIC(root, d, "d", 2u);
  PW_METRIC(root, f, "f", 3u);

  PW_METRIC_GROUP(inner_1, "inner1");
  PW_METRIC(inner_1, x, "x", 4u);
  PW_METRIC(inner_1, y, "y", 5u);
  PW_METRIC(inner_1, z, "z", 6u);

  PW_METRIC_GROUP(inner_2, "inner2");
  PW_METRIC(inner_2, p, "p", 7u);
  PW_METRIC(inner_2, q, "q", 8u);
  PW_METRIC(inner_2, r, "r", 9u);
  PW_METRIC(inner_2, s, "s", 10u);  // Note: Max # per response is 10.
  PW_METRIC(inner_2, t, "s", 11u);
  PW_METRIC(inner_2, u, "s", 12u);

  root.Add(inner_1);
  root.Add(inner_2);

  // Run the RPC and ensure it completes.
  MetricMethodContext context(root.metrics(), root.children());
  context.call({});
  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());

  // The response had to be split into two parts; check that they have the
  // appropriate sizes.
  EXPECT_EQ(2u, context.responses().size());
  EXPECT_EQ(10, context.responses()[0].metrics_count);
  EXPECT_EQ(2, context.responses()[1].metrics_count);

  // The metrics are the numbers 1..12; sum them and compare.
  uint32_t metric_sum = 0;
  for (const auto& response : context.responses()) {
    for (unsigned i = 0; i < response.metrics_count; ++i) {
      metric_sum += response.metrics[i].value.as_int;
    }
  }
  EXPECT_EQ(78u, metric_sum);

  // TODO(keir): Properly check all the fields.
}

}  // namespace
}  // namespace pw::metric
