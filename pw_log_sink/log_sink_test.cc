// Copyright 2021 The Pigweed Authors
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

#include "pw_log_sink/log_sink.h"

#include <string>

#include "gtest/gtest.h"
#include "pw_log/levels.h"
#include "pw_log_proto/log.pwpb.h"
#include "pw_log_sink/multisink_adapter.h"
#include "pw_multisink/drain.h"
#include "pw_multisink/multisink.h"
#include "pw_protobuf/decoder.h"

namespace pw::log_sink {
namespace {
constexpr size_t kMaxTokenizedMessageSize = 512;
constexpr char kTokenizedMessage[] = "Test Message";

std::string LogMessageToString(ConstByteSpan message) {
  pw::protobuf::Decoder log_decoder(message);
  std::string_view log_entry_message;

  EXPECT_TRUE(log_decoder.Next().ok());  // line_level - 2
  EXPECT_TRUE(log_decoder.Next().ok());  // flags - 3
  EXPECT_TRUE(log_decoder.Next().ok());  // timestamp - 5
  EXPECT_TRUE(log_decoder.Next().ok());  // message_string - 16
  EXPECT_EQ(16U, log_decoder.FieldNumber());
  EXPECT_TRUE(log_decoder.ReadString(&log_entry_message).ok());

  return std::string(log_entry_message);
}

void LogMessageForTest(const char* message) {
  pw_LogSink_Log(0, 0, nullptr, nullptr, 0, nullptr, "%s", message);
}

void LogInvalidMessageForTest() {
  char long_message[kMaxTokenizedMessageSize + 1];
  memset(long_message, 'A', sizeof(long_message));
  long_message[kMaxTokenizedMessageSize] = '\0';

  pw_LogSink_Log(0, 0, nullptr, nullptr, 0, nullptr, "%s", long_message);
}

class TestSink final : public Sink {
 public:
  void HandleEntry(ConstByteSpan message) final {
    last_message_string_ = LogMessageToString(message);
    message_count_++;
  }

  void HandleDropped(uint32_t count) final { drop_count_ += count; }

  void VerifyMessage(std::string tokenized_message) {
    EXPECT_EQ(tokenized_message, last_message_string_);
  }

  uint32_t GetMessageCount() { return message_count_; }
  uint32_t GetDropCount() { return drop_count_; }

 private:
  std::string last_message_string_;
  uint32_t message_count_ = 0;
  uint32_t drop_count_ = 0;
};

}  // namespace

TEST(LogSink, NoSink) {
  // Confirm that the log function does not crash when no sink is present.
  LogMessageForTest(kTokenizedMessage);
}

TEST(LogSink, SingleSink) {
  TestSink test_sink;

  AddSink(test_sink);
  LogMessageForTest(kTokenizedMessage);
  test_sink.VerifyMessage(kTokenizedMessage);
  RemoveSink(test_sink);
}

TEST(LogSink, MultipleSink) {
  TestSink test_sink_io;
  TestSink test_sink_bt;

  AddSink(test_sink_io);
  AddSink(test_sink_bt);
  LogMessageForTest(kTokenizedMessage);
  test_sink_io.VerifyMessage(kTokenizedMessage);
  test_sink_bt.VerifyMessage(kTokenizedMessage);

  RemoveSink(test_sink_io);
  LogMessageForTest(kTokenizedMessage);
  test_sink_io.VerifyMessage(kTokenizedMessage);
  EXPECT_EQ(test_sink_io.GetMessageCount(), 1U);
  EXPECT_EQ(test_sink_bt.GetMessageCount(), 2U);

  RemoveSink(test_sink_bt);
}

TEST(LogSink, DroppedEntries) {
  TestSink test_sink;

  LogMessageForTest(kTokenizedMessage);

  AddSink(test_sink);
  LogMessageForTest(kTokenizedMessage);
  EXPECT_EQ(test_sink.GetMessageCount(), 1U);
  EXPECT_EQ(test_sink.GetDropCount(), 1U);

  LogInvalidMessageForTest();
  EXPECT_EQ(test_sink.GetMessageCount(), 1U);
  EXPECT_EQ(test_sink.GetDropCount(), 2U);

  LogMessageForTest(kTokenizedMessage);
  EXPECT_EQ(test_sink.GetMessageCount(), 2U);
  EXPECT_EQ(test_sink.GetDropCount(), 2U);

  RemoveSink(test_sink);
}

TEST(LogSink, MultiSinkAdapter) {
  constexpr size_t kMultiSinkBufferSize = 1024;
  std::byte buffer[kMultiSinkBufferSize];
  std::byte entry_buffer[kMultiSinkBufferSize];
  pw::multisink::MultiSink multisink(buffer);
  pw::multisink::Drain drain;
  MultiSinkAdapter multisink_adapter(multisink);

  multisink.AttachDrain(drain);
  LogMessageForTest(kTokenizedMessage);

  AddSink(multisink_adapter);
  LogMessageForTest(kTokenizedMessage);

  uint32_t drop_count = 0;
  Result<ConstByteSpan> entry = drain.GetEntry(entry_buffer, drop_count);
  ASSERT_TRUE(entry.ok());
  EXPECT_EQ(LogMessageToString(entry.value()), kTokenizedMessage);
  EXPECT_EQ(drop_count, 1U);
}

}  // namespace pw::log_sink
