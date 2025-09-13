// Copyright 2025 The Pigweed Authors
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

#include "pw_metric/metric_service_pwpb.h"

#include "pw_bytes/span.h"
#include "pw_log/log.h"
#include "pw_metric_proto/metric_service.pwpb.h"
#include "pw_protobuf/decoder.h"
#include "pw_protobuf/encoder.h"
#include "pw_protobuf/serialized_size.h"
#include "pw_rpc/pwpb/test_method_context.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_rpc/test_helpers.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace pw::metric {
namespace {

size_t CountEncodedMetrics(ConstByteSpan serialized_path) {
  protobuf::Decoder decoder(serialized_path);
  size_t num_metrics = 0;
  while (decoder.Next().ok()) {
    switch (decoder.FieldNumber()) {
      case static_cast<uint32_t>(
          proto::pwpb::MetricResponse::Fields::kMetrics): {
        num_metrics++;
      }
    }
  }
  return num_metrics;
}

size_t SumMetricInts(ConstByteSpan serialized_path) {
  protobuf::Decoder decoder(serialized_path);
  size_t metrics_sum = 0;
  while (decoder.Next().ok()) {
    switch (decoder.FieldNumber()) {
      case static_cast<uint32_t>(proto::pwpb::Metric::Fields::kAsInt): {
        uint32_t metric_value;
        EXPECT_EQ(OkStatus(), decoder.ReadUint32(&metric_value));
        metrics_sum += metric_value;
      }
    }
  }
  return metrics_sum;
}

size_t GetMetricsSum(ConstByteSpan serialized_metric_buffer) {
  protobuf::Decoder decoder(serialized_metric_buffer);
  size_t metrics_sum = 0;
  while (decoder.Next().ok()) {
    switch (decoder.FieldNumber()) {
      case static_cast<uint32_t>(
          proto::pwpb::MetricResponse::Fields::kMetrics): {
        ConstByteSpan metric_buffer;
        EXPECT_EQ(OkStatus(), decoder.ReadBytes(&metric_buffer));
        metrics_sum += SumMetricInts(metric_buffer);
      }
    }
  }
  return metrics_sum;
}

//
// Legacy Get() RPC Tests
//

TEST(MetricService, EmptyGroupAndNoMetrics) {
  // Empty root group.
  PW_METRIC_GROUP(root, "/");

  // Run the RPC and ensure it completes.

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Get)
  ctx{root.metrics(), root.children()};
  ctx.call({});
  EXPECT_TRUE(ctx.done());
  EXPECT_EQ(OkStatus(), ctx.status());

  // No metrics should be in the response.
  EXPECT_EQ(0u, ctx.responses().size());
}

TEST(MetricService, OneGroupOneMetric) {
  // One root group with one metric.
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 3u);

  // Run the RPC and ensure it completes.

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Get)
  ctx{root.metrics(), root.children()};
  ctx.call({});
  EXPECT_TRUE(ctx.done());
  EXPECT_EQ(OkStatus(), ctx.status());

  // One metric should be in the response.
  EXPECT_EQ(1u, ctx.responses().size());

  // Sum should be 3.
  EXPECT_EQ(3u, GetMetricsSum(ctx.responses()[0]));
}

TEST(MetricService, OneGroupFiveMetrics) {
  // One root group with five metrics.
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1u);
  PW_METRIC(root, b, "b", 2u);  // Note: Max # per response is 3.
  PW_METRIC(root, c, "c", 3u);
  PW_METRIC(root, x, "x", 4u);
  PW_METRIC(root, y, "y", 5u);

  // Run the RPC and ensure it completes.

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Get)
  ctx{root.metrics(), root.children()};
  ctx.call({});
  EXPECT_TRUE(ctx.done());
  EXPECT_EQ(OkStatus(), ctx.status());

  // Two metrics should be in the response.
  EXPECT_EQ(2u, ctx.responses().size());
  EXPECT_EQ(3u, CountEncodedMetrics(ctx.responses()[0]));
  EXPECT_EQ(2u, CountEncodedMetrics(ctx.responses()[1]));

  // The metrics are the numbers 1..5; sum them and compare.
  EXPECT_EQ(
      15u,
      GetMetricsSum(ctx.responses()[0]) + GetMetricsSum(ctx.responses()[1]));
}

TEST(MetricService, NestedGroupFiveMetrics) {
  // Set up a nested group of metrics.
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1u);
  PW_METRIC(root, b, "b", 2u);

  PW_METRIC_GROUP(inner, "inner");
  PW_METRIC(root, x, "x", 3u);  // Note: Max # per response is 3.
  PW_METRIC(inner, y, "y", 4u);
  PW_METRIC(inner, z, "z", 5u);

  root.Add(inner);

  // Run the RPC and ensure it completes.

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Get)
  ctx{root.metrics(), root.children()};
  ctx.call({});
  EXPECT_TRUE(ctx.done());
  EXPECT_EQ(OkStatus(), ctx.status());

  // Two metrics should be in the response.
  EXPECT_EQ(2u, ctx.responses().size());
  EXPECT_EQ(3u, CountEncodedMetrics(ctx.responses()[0]));
  EXPECT_EQ(2u, CountEncodedMetrics(ctx.responses()[1]));

  EXPECT_EQ(
      15u,
      GetMetricsSum(ctx.responses()[0]) + GetMetricsSum(ctx.responses()[1]));
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
  PW_METRIC(inner_2, s, "s", 10u);  // Note: Max # per response is 3.
  PW_METRIC(inner_2, t, "t", 11u);
  PW_METRIC(inner_2, u, "u", 12u);

  root.Add(inner_1);
  root.Add(inner_2);

  // Run the RPC and ensure it completes.
  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Get)
  ctx{root.metrics(), root.children()};
  ctx.call({});
  EXPECT_TRUE(ctx.done());
  EXPECT_EQ(OkStatus(), ctx.status());

  // The response had to be split into four parts; check that they have the
  // appropriate sizes.
  EXPECT_EQ(4u, ctx.responses().size());
  EXPECT_EQ(3u, CountEncodedMetrics(ctx.responses()[0]));
  EXPECT_EQ(3u, CountEncodedMetrics(ctx.responses()[1]));
  EXPECT_EQ(3u, CountEncodedMetrics(ctx.responses()[2]));
  EXPECT_EQ(3u, CountEncodedMetrics(ctx.responses()[3]));

  EXPECT_EQ(78u,
            GetMetricsSum(ctx.responses()[0]) +
                GetMetricsSum(ctx.responses()[1]) +
                GetMetricsSum(ctx.responses()[2]) +
                GetMetricsSum(ctx.responses()[3]));
}

TEST(MetricService, MaxDepth4) {
  // MetricWalker internally uses: Vector<Token, /*capacity=*/4> path_;
  // pw.metric.proto.Metric.token_path max_count:4

  IntrusiveList<Group> global_groups;    // Simulate pw::metric::global_groups
  IntrusiveList<Metric> global_metrics;  // Simulate pw::metric::global_metrics

  PW_METRIC_GROUP(global_group_lvl1, "level1");
  global_groups.push_back(global_group_lvl1);

  PW_METRIC_GROUP(global_group_lvl1, group_lvl2, "level2");
  PW_METRIC_GROUP(group_lvl2, group_lvl3, "level3");

  // Note: kMaxNumPackedEntries = 3
  PW_METRIC(group_lvl3, metric_a, "metric A", 1u);
  PW_METRIC(group_lvl3, metric_b, "metric B", 2u);
  PW_METRIC(group_lvl3, metric_c, "metric C", 3u);

  // Run the RPC and ensure it completes.
  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Get)
  ctx{global_metrics, global_groups};
  ctx.call({});
  EXPECT_TRUE(ctx.done());
  EXPECT_EQ(OkStatus(), ctx.status());

  // Verify the response
  EXPECT_EQ(1u, ctx.responses().size());
  EXPECT_EQ(3u, CountEncodedMetrics(ctx.responses()[0]));
  EXPECT_EQ(6u, GetMetricsSum(ctx.responses()[0]));
}

//
// Walk() RPC Tests
//

// Helper function to precisely calculate the encoded size of a metric as it
// would appear in a WalkResponse. This is critical for setting up deterministic
// pagination tests.
size_t GetEncodedMetricSize(const Metric& metric, const Vector<Token>& path) {
  // A packed repeated fixed32 field (like token_path) is encoded on the wire
  // identically to a bytes field. First, calculate the size of the payload.
  const size_t token_path_payload_size = path.size() * sizeof(uint32_t);

  // Now, calculate the total size of the token_path field within the Metric
  // message, including its tag and length prefix.
  size_t metric_payload_size = protobuf::SizeOfDelimitedField(
      proto::pwpb::Metric::Fields::kTokenPath, token_path_payload_size);

  if (metric.is_float()) {
    metric_payload_size +=
        protobuf::SizeOfFieldFloat(proto::pwpb::Metric::Fields::kAsFloat);
  } else {
    metric_payload_size += protobuf::SizeOfFieldUint32(
        proto::pwpb::Metric::Fields::kAsInt, metric.as_int());
  }

  // Calculate the size of the entire Metric message when encoded as a field
  // within the WalkResponse.
  return protobuf::SizeOfDelimitedField(
      proto::pwpb::WalkResponse::Fields::kMetrics, metric_payload_size);
}

size_t CountMetricsInWalkResponse(ConstByteSpan serialized_response) {
  protobuf::Decoder decoder(serialized_response);
  size_t num_metrics = 0;
  while (decoder.Next().ok()) {
    if (decoder.FieldNumber() ==
        static_cast<uint32_t>(proto::pwpb::WalkResponse::Fields::kMetrics)) {
      ++num_metrics;
    }
  }
  return num_metrics;
}

TEST(MetricService, Walk) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1u);
  PW_METRIC(root, b, "b", 2u);
  PW_METRIC_GROUP(inner, "inner");
  PW_METRIC(inner, x, "x", 3u);
  root.Add(inner);

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
  ctx{root.metrics(), root.children()};

  // Manually encode the request.
  std::array<std::byte, 32> request_buffer;
  proto::pwpb::WalkRequest::MemoryEncoder request_encoder(request_buffer);
  ASSERT_EQ(OkStatus(), request_encoder.Write({}));
  ctx.call(request_encoder);
  EXPECT_EQ(OkStatus(), ctx.status());

  // Manually decode and iterate over the response.
  protobuf::Decoder decoder(ctx.response());
  size_t total_metrics = 0;
  bool done = false;
  bool has_cursor = false;

  while (decoder.Next().ok()) {
    switch (
        static_cast<proto::pwpb::WalkResponse::Fields>(decoder.FieldNumber())) {
      case proto::pwpb::WalkResponse::Fields::kMetrics:
        total_metrics++;
        break;
      case proto::pwpb::WalkResponse::Fields::kDone:
        ASSERT_EQ(decoder.ReadBool(&done), OkStatus());
        break;
      case proto::pwpb::WalkResponse::Fields::kCursor:
        has_cursor = true;
        break;
      default:
        break;
    }
  }

  EXPECT_EQ(3u, total_metrics);
  EXPECT_TRUE(done);
  EXPECT_FALSE(has_cursor);
}

TEST(MetricService, WalkWithPagination) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, m0, "m0", 0u);
  PW_METRIC(root, m1, "m1", 1u);
  PW_METRIC(root, m2, "m2", 2u);
  PW_METRIC(root, m3, "m3", 3u);
  PW_METRIC(root, m4, "m4", 4u);

  Vector<Token, 2> path;
  path.push_back(root.name());
  path.push_back(m0.name());  // Path is same for all metrics here.

  const size_t size_one_metric = GetEncodedMetricSize(m0, path);
  constexpr size_t kWalkResponseOverhead =
      protobuf::SizeOfFieldUint64(proto::pwpb::WalkResponse::Fields::kCursor);

  // The RPC framework reserves this many bytes for its own packet headers. This
  // was determined empirically through logging.
  constexpr size_t kRpcOverhead = 32;

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
  ctx{root.metrics(), root.children()};

  // Set the MTU to be large enough for exactly three metrics and the payload
  // overhead, plus the RPC overhead.
  const size_t payload_capacity = (3 * size_one_metric) + kWalkResponseOverhead;
  const size_t mtu = payload_capacity + kRpcOverhead;
  ctx.output().set_mtu(mtu);

  size_t total_metrics = 0;
  uint64_t cursor = 0;

  for (int i = 0; i < 5; ++i) {  // Loop to prevent infinite loops from bugs.
    std::array<std::byte, 32> request_buffer;
    proto::pwpb::WalkRequest::MemoryEncoder request_encoder(request_buffer);
    ASSERT_EQ(OkStatus(), request_encoder.Write({.cursor = cursor}));
    ctx.call(request_encoder);
    EXPECT_EQ(OkStatus(), ctx.status());

    total_metrics += CountMetricsInWalkResponse(ctx.response());

    protobuf::Decoder decoder(ctx.response());
    bool done = false;
    cursor = 0;

    while (decoder.Next().ok()) {
      switch (static_cast<proto::pwpb::WalkResponse::Fields>(
          decoder.FieldNumber())) {
        case proto::pwpb::WalkResponse::Fields::kMetrics:
          break;  // Already counted
        case proto::pwpb::WalkResponse::Fields::kDone:
          ASSERT_EQ(decoder.ReadBool(&done), OkStatus());
          break;
        case proto::pwpb::WalkResponse::Fields::kCursor:
          ASSERT_EQ(decoder.ReadUint64(&cursor), OkStatus());
          break;
        default:
          break;
      }
    }

    if (done) {
      EXPECT_EQ(cursor, 0u);
      break;
    }
    ctx.output().clear();
  }

  EXPECT_EQ(total_metrics, 5u);
}

TEST(MetricService, WalkWithInvalidCursor) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, a, "a", 1u);

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
  ctx{root.metrics(), root.children()};

  std::array<std::byte, 32> request_buffer;
  proto::pwpb::WalkRequest::MemoryEncoder request_encoder(request_buffer);
  ASSERT_EQ(OkStatus(), request_encoder.Write({.cursor = 12345}));

  ctx.call(request_encoder);
  EXPECT_EQ(Status::NotFound(), ctx.status());
}

TEST(MetricService, WalkWithStaleCursorAfterMutation) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, m0, "m0", 0u);
  PW_METRIC(root, m1, "m1", 1u);

  uint64_t response_cursor = 0;
  // Create a scope for the first RPC context.
  {
    PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
    ctx{root.metrics(), root.children()};

    // Set a small MTU to force pagination to occur after a single metric.
    constexpr size_t kRpcOverhead = 32;
    Vector<Token, 2> path;
    path.push_back(root.name());
    path.push_back(m1.name());
    const size_t size_m1 = GetEncodedMetricSize(m1, path);
    constexpr size_t kWalkResponseOverhead =
        protobuf::SizeOfFieldUint64(proto::pwpb::WalkResponse::Fields::kCursor);
    const size_t mtu = size_m1 + kWalkResponseOverhead + kRpcOverhead;
    ctx.output().set_mtu(mtu);

    // First page.
    std::array<std::byte, 32> request_buffer;
    proto::pwpb::WalkRequest::MemoryEncoder request_encoder(request_buffer);
    ASSERT_EQ(OkStatus(), request_encoder.Write({}));
    ctx.call(request_encoder);
    ASSERT_EQ(OkStatus(), ctx.status());

    protobuf::Decoder decoder(ctx.response());
    bool found_cursor = false;
    while (decoder.Next().ok()) {
      if (decoder.FieldNumber() ==
          static_cast<uint32_t>(proto::pwpb::WalkResponse::Fields::kCursor)) {
        ASSERT_EQ(OkStatus(), decoder.ReadUint64(&response_cursor));
        found_cursor = true;
      }
    }
    ASSERT_TRUE(found_cursor);
  }

  // Due to push_front, the list order is [m1, m0]. The walker processes m1,
  // and the cursor for the next page points to m0.
  // Mutate the tree: remove the metric the cursor points to.
  ASSERT_TRUE(root.metrics().remove(m0));

  // Second page: Use the now-stale cursor within a new context.
  {
    PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
    ctx{root.metrics(), root.children()};

    std::array<std::byte, 32> request_buffer;
    proto::pwpb::WalkRequest::MemoryEncoder request_encoder(request_buffer);
    ASSERT_EQ(OkStatus(), request_encoder.Write({.cursor = response_cursor}));
    ctx.call(request_encoder);

    // This call must fail because the metric at the cursor address is gone.
    EXPECT_EQ(Status::NotFound(), ctx.status());
  }
}

TEST(MetricService, WalkPaginatesCorrectlyWhenPageIsFull) {
  PW_METRIC_GROUP(root, "/");
  PW_METRIC(root, m0, "m0", 0u);
  PW_METRIC(root, m1, "m1", 1u);
  PW_METRIC(root, m2, "m2", 2u);

  Vector<Token, 2> path_m2;
  path_m2.push_back(root.name());
  path_m2.push_back(m2.name());

  Vector<Token, 2> path_m1;
  path_m1.push_back(root.name());
  path_m1.push_back(m1.name());

  const size_t size_m2 = GetEncodedMetricSize(m2, path_m2);
  const size_t size_m1 = GetEncodedMetricSize(m1, path_m1);

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
  ctx{root.metrics(), root.children()};

  // The RPC framework reserves this many bytes for its own packet headers.
  constexpr size_t kRpcOverhead = 32;
  constexpr size_t kWalkResponseOverhead =
      protobuf::SizeOfFieldUint64(proto::pwpb::WalkResponse::Fields::kCursor);

  // Set the MTU to be large enough for exactly two metrics and the payload
  // overhead, plus the RPC overhead. This forces pagination after two metrics.
  const size_t payload_capacity = size_m2 + size_m1 + kWalkResponseOverhead;
  const size_t mtu = payload_capacity + kRpcOverhead;
  ctx.output().set_mtu(mtu);

  // The first page should contain only the first two metrics processed (m2,
  // m1 because of intrusive list order).
  std::array<std::byte, 32> request_buffer;
  proto::pwpb::WalkRequest::MemoryEncoder request_encoder(request_buffer);
  ASSERT_EQ(OkStatus(), request_encoder.Write({}));
  ctx.call(request_encoder);
  ASSERT_EQ(OkStatus(), ctx.status());

  protobuf::Decoder decoder(ctx.response());
  size_t metric_count = 0;
  uint64_t cursor = 0;
  bool done = true;
  while (decoder.Next().ok()) {
    if (decoder.FieldNumber() ==
        static_cast<uint32_t>(proto::pwpb::WalkResponse::Fields::kMetrics)) {
      metric_count++;
    }
    if (decoder.FieldNumber() ==
        static_cast<uint32_t>(proto::pwpb::WalkResponse::Fields::kCursor)) {
      ASSERT_EQ(OkStatus(), decoder.ReadUint64(&cursor));
    }
    if (decoder.FieldNumber() ==
        static_cast<uint32_t>(proto::pwpb::WalkResponse::Fields::kDone)) {
      ASSERT_EQ(OkStatus(), decoder.ReadBool(&done));
    }
  }

  // Verify that only two metrics were included.
  EXPECT_EQ(metric_count, 2u);
  // Verify that the cursor points to the metric that didn't fit (m0).
  EXPECT_EQ(cursor, reinterpret_cast<uint64_t>(&m0));
  EXPECT_FALSE(done);
}

TEST(MetricService, WalkWithMaxDepth) {
  PW_METRIC_GROUP(root, "l0");
  PW_METRIC_GROUP(root, l1, "l1");
  PW_METRIC_GROUP(l1, l2, "l2");
  PW_METRIC(l2, a, "a", 1u);

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
  ctx{root.metrics(), root.children()};
  ctx.call({});
  EXPECT_EQ(OkStatus(), ctx.status());
}

#if GTEST_HAS_DEATH_TEST
TEST(MetricService, WalkWithMaxDepthExceeded) {
  PW_METRIC_GROUP(root, "l0");
  PW_METRIC_GROUP(root, l1, "l1");
  PW_METRIC_GROUP(l1, l2, "l2");
  PW_METRIC_GROUP(l2, l3, "l3");
  PW_METRIC_GROUP(l3, l4, "l4");
  PW_METRIC(l4, a, "a", 1u);

  PW_RAW_TEST_METHOD_CONTEXT(MetricService, Walk)
  ctx{root.metrics(), root.children()};
  EXPECT_DEATH_IF_SUPPORTED(static_cast<void>(ctx.call({})), ".*");
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace
}  // namespace pw::metric
