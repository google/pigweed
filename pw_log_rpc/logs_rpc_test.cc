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

#include "pw_log_rpc/logs_rpc.h"

#include "gtest/gtest.h"
#include "pw_log/log.h"
#include "pw_rpc/raw_test_method_context.h"

namespace pw::log_rpc {
namespace {

#define LOGS_METHOD_CONTEXT PW_RAW_TEST_METHOD_CONTEXT(Logs, Get)

constexpr size_t kEncodeBufferSize = 128;
constexpr size_t kLogBufferSize = 4096;

class LogQueueTester : public LogQueueWithEncodeBuffer<kLogBufferSize> {
 public:
  LogQueueTester(ByteSpan log_queue)
      : LogQueueWithEncodeBuffer<kLogBufferSize>(log_queue) {}

  void SetPopStatus(Status error_status) {
    pop_status_for_test_ = error_status;
  }
};

class LogsService : public ::testing::Test {
 public:
  LogsService() : log_queue_(log_queue_buffer_) {}

 protected:
  void AddLogs(const size_t log_count = 1) {
    constexpr char kTokenizedMessage[] = "message";
    for (size_t i = 0; i < log_count; i++) {
      EXPECT_EQ(
          Status::Ok(),
          log_queue_.PushTokenizedMessage(
              std::as_bytes(std::span(kTokenizedMessage)), 0, 0, 0, 0, 0));
    }
  }

  static Logs& GetLogs(LOGS_METHOD_CONTEXT& context) {
    return (Logs&)(context.service());
  }

  std::array<std::byte, kEncodeBufferSize> log_queue_buffer_;
  LogQueueWithEncodeBuffer<kLogBufferSize> log_queue_;
};

TEST_F(LogsService, Get) {
  constexpr size_t kLogEntryCount = 3;
  std::array<std::byte, 1> rpc_buffer;
  LOGS_METHOD_CONTEXT context(log_queue_);

  context.call(rpc_buffer);

  // Flush all logs from the buffer, then close the RPC.
  AddLogs(kLogEntryCount);
  GetLogs(context).Flush();
  GetLogs(context).Finish();

  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());

  // Although |kLogEntryCount| messages were in the queue, they are batched
  // before being written to the client, so there is only one response.
  EXPECT_EQ(1U, context.total_responses());
}

TEST_F(LogsService, GetMultiple) {
  constexpr size_t kLogEntryCount = 1;
  constexpr size_t kFlushCount = 3;
  std::array<std::byte, 1> rpc_buffer;
  LOGS_METHOD_CONTEXT context(log_queue_);

  context.call(rpc_buffer);

  for (size_t i = 0; i < kFlushCount; i++) {
    AddLogs(kLogEntryCount);
    GetLogs(context).Flush();
  }
  GetLogs(context).Finish();

  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());
  EXPECT_EQ(kFlushCount, context.total_responses());
}

TEST_F(LogsService, NoEntriesOnEmptyQueue) {
  std::array<std::byte, 1> rpc_buffer;
  LOGS_METHOD_CONTEXT context(log_queue_);

  // Invoking flush with no logs in the queue should behave like a no-op.
  context.call(rpc_buffer);
  GetLogs(context).Flush();
  GetLogs(context).Finish();

  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());
  EXPECT_EQ(0U, context.total_responses());
}

TEST_F(LogsService, QueueError) {
  std::array<std::byte, 1> rpc_buffer;
  LogQueueTester log_queue_tester(log_queue_buffer_);
  LOGS_METHOD_CONTEXT context(log_queue_tester);

  // Generate failure on log queue.
  log_queue_tester.SetPopStatus(Status::Internal());
  context.call(rpc_buffer);
  GetLogs(context).Flush();
  GetLogs(context).Finish();

  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Ok(), context.status());
  EXPECT_EQ(0U, context.total_responses());
}

}  // namespace
}  // namespace pw::log_rpc
