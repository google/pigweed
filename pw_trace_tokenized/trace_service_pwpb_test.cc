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

#include "pw_trace_tokenized/trace_service_pwpb.h"

#include "pw_chrono/system_clock.h"
#include "pw_rpc/pwpb/test_method_context.h"
#include "pw_stream/memory_stream.h"
#include "pw_trace/trace.h"
#include "pw_trace_tokenized/trace_tokenized.h"
#include "pw_unit_test/framework.h"

namespace pw::trace {

class TraceServiceTest : public ::testing::Test {
 public:
  TraceServiceTest() {}

  static constexpr uint32_t kTraceTransferHandlerId = 7;
};

TEST_F(TraceServiceTest, Start) {
  auto& tracer = trace::GetTokenizedTracer();

  std::array<std::byte, PW_TRACE_BUFFER_SIZE_BYTES> dest_buffer;
  stream::MemoryWriter writer(dest_buffer);
  PW_PWPB_TEST_METHOD_CONTEXT(TraceService, Start)
  context(tracer, writer);

  ASSERT_FALSE(tracer.IsEnabled());
  ASSERT_EQ(context.call({}), OkStatus());
  ASSERT_TRUE(tracer.IsEnabled());

  // multiple calls to start are disallowed
  ASSERT_EQ(context.call({}), Status::FailedPrecondition());
}

TEST_F(TraceServiceTest, Stop) {
  auto& tracer = trace::GetTokenizedTracer();

  std::array<std::byte, PW_TRACE_BUFFER_SIZE_BYTES> dest_buffer;
  stream::MemoryWriter writer(dest_buffer);
  PW_PWPB_TEST_METHOD_CONTEXT(TraceService, Stop)
  context(tracer, writer);
  context.service().SetTransferId(kTraceTransferHandlerId);

  tracer.Enable(true);
  PW_TRACE_INSTANT("TestTrace");

  ASSERT_EQ(context.call({}), OkStatus());
  ASSERT_FALSE(tracer.IsEnabled());
  EXPECT_EQ(kTraceTransferHandlerId, context.response().file_id);
  EXPECT_LT(0u, writer.bytes_written());
}

TEST_F(TraceServiceTest, StopNoTransferHandlerId) {
  auto& tracer = trace::GetTokenizedTracer();

  std::array<std::byte, PW_TRACE_BUFFER_SIZE_BYTES> dest_buffer;
  stream::MemoryWriter writer(dest_buffer);
  PW_PWPB_TEST_METHOD_CONTEXT(TraceService, Stop)
  context(tracer, writer);

  tracer.Enable(true);
  PW_TRACE_INSTANT("TestTrace");

  ASSERT_EQ(context.call({}), OkStatus());
  ASSERT_FALSE(tracer.IsEnabled());
  EXPECT_FALSE(context.response().file_id.has_value());
  EXPECT_LT(0u, writer.bytes_written());
}

TEST_F(TraceServiceTest, StopNotStarted) {
  auto& tracer = trace::GetTokenizedTracer();

  std::array<std::byte, PW_TRACE_BUFFER_SIZE_BYTES> dest_buffer;
  stream::MemoryWriter writer(dest_buffer);
  PW_PWPB_TEST_METHOD_CONTEXT(TraceService, Stop)
  context(tracer, writer);

  // stopping while tracing is disabled results in FailedPrecondition
  ASSERT_EQ(context.call({}), Status::FailedPrecondition());
}

TEST_F(TraceServiceTest, StopNoData) {
  auto& tracer = trace::GetTokenizedTracer();

  std::array<std::byte, PW_TRACE_BUFFER_SIZE_BYTES> dest_buffer;
  stream::MemoryWriter writer(dest_buffer);
  PW_PWPB_TEST_METHOD_CONTEXT(TraceService, Stop)
  context(tracer, writer);

  tracer.Enable(true);

  // stopping with no trace data results in Unavailable
  ASSERT_EQ(context.call({}), Status::Unavailable());
}

TEST_F(TraceServiceTest, GetClockParameters) {
  auto& tracer = trace::GetTokenizedTracer();

  std::array<std::byte, PW_TRACE_BUFFER_SIZE_BYTES> dest_buffer;
  stream::MemoryWriter writer(dest_buffer);

  PW_PWPB_TEST_METHOD_CONTEXT(TraceService, GetClockParameters)
  context(tracer, writer);

  ASSERT_EQ(context.call({}), OkStatus());
  EXPECT_EQ(PW_CHRONO_SYSTEM_CLOCK_PERIOD_SECONDS_NUMERATOR,
            context.response().clock_parameters.tick_period_seconds_numerator);
  EXPECT_EQ(
      PW_CHRONO_SYSTEM_CLOCK_PERIOD_SECONDS_DENOMINATOR,
      context.response().clock_parameters.tick_period_seconds_denominator);
  EXPECT_EQ(
      static_cast<int32_t>(chrono::SystemClock::epoch),
      static_cast<int32_t>(*context.response().clock_parameters.epoch_type));
}

}  // namespace pw::trace
