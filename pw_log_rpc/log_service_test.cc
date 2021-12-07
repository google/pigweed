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

#include "pw_log_rpc/log_service.h"

#include <array>
#include <cstdint>
#include <limits>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_bytes/endian.h"
#include "pw_containers/vector.h"
#include "pw_log/log.h"
#include "pw_log/proto/log.pwpb.h"
#include "pw_log/proto_utils.h"
#include "pw_log_rpc/log_filter.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_protobuf/bytes_utils.h"
#include "pw_protobuf/decoder.h"
#include "pw_result/result.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/raw/fake_channel_output.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_sync/mutex.h"

namespace pw::log_rpc {
namespace {

using log::pw_rpc::raw::Logs;

#define LOG_SERVICE_METHOD_CONTEXT \
  PW_RAW_TEST_METHOD_CONTEXT(LogService, Listen, 10, 256)

constexpr size_t kMaxMessageSize = 50;
constexpr size_t kMaxLogEntrySize =
    RpcLogDrain::kMinEntrySizeWithoutPayload + kMaxMessageSize;
static_assert(RpcLogDrain::kMinEntryBufferSize < kMaxLogEntrySize);
constexpr size_t kMultiSinkBufferSize = kMaxLogEntrySize * 10;
constexpr size_t kMaxDrains = 3;
constexpr char kMessage[] = "message";
// A message small enough to fit encoded in LogServiceTest::entry_encode_buffer_
// but large enough to not fit in LogServiceTest::small_buffer_.
constexpr char kLongMessage[] =
    "This is a long log message that will be dropped.";
static_assert(sizeof(kLongMessage) < kMaxMessageSize);
static_assert(sizeof(kLongMessage) > RpcLogDrain::kMinEntryBufferSize);
std::array<std::byte, 1> rpc_request_buffer;
constexpr auto kSampleMetadata =
    log_tokenized::Metadata::Set<PW_LOG_LEVEL_INFO, 123, 0x03, __LINE__>();
constexpr auto kDropMessageMetadata =
    log_tokenized::Metadata::Set<0, 0, 0, 0>();
constexpr int64_t kSampleTimestamp = 1000;

// `LogServiceTest` sets up a logging environment for testing with a `MultiSink`
// for log entries, and multiple `RpcLogDrain`s for consuming such log entries.
// It includes methods to add log entries to the `MultiSink`, and buffers for
// encoding and retrieving log entries. Tests can choose how many entries to
// add to the multisink, and which drain to use.
class LogServiceTest : public ::testing::Test {
 public:
  LogServiceTest()
      : multisink_(multisink_buffer_),
        drain_map_(drains_),
        filter_map_(filters_) {
    for (auto& drain : drain_map_.drains()) {
      multisink_.AttachDrain(drain);
    }
  }

  void AddLogEntries(size_t log_count,
                     std::string_view message,
                     log_tokenized::Metadata metadata,
                     int64_t timestamp) {
    for (size_t i = 0; i < log_count; ++i) {
      ASSERT_TRUE(AddLogEntry(message, metadata, timestamp).ok());
    }
  }

  StatusWithSize AddLogEntry(std::string_view message,
                             log_tokenized::Metadata metadata,
                             int64_t timestamp) {
    Result<ConstByteSpan> encoded_log_result =
        log::EncodeTokenizedLog(metadata,
                                std::as_bytes(std::span(message)),
                                timestamp,
                                entry_encode_buffer_);
    PW_TRY_WITH_SIZE(encoded_log_result.status());
    multisink_.HandleEntry(encoded_log_result.value());
    return StatusWithSize(encoded_log_result.value().size());
  }

 protected:
  std::array<std::byte, kMultiSinkBufferSize> multisink_buffer_;
  multisink::MultiSink multisink_;
  RpcLogDrainMap drain_map_;
  std::array<std::byte, kMaxLogEntrySize> entry_encode_buffer_;
  FilterMap filter_map_;
  static constexpr size_t kMaxFilterRules = 4;
  std::array<Filter::Rule, kMaxFilterRules> rules1_;
  std::array<Filter::Rule, kMaxFilterRules> rules2_;
  std::array<Filter::Rule, kMaxFilterRules> rules3_;
  static constexpr std::array<std::byte, cfg::kMaxFilterIdBytes> filter_id1_{
      std::byte(65), std::byte(66), std::byte(67), std::byte(0)};
  static constexpr std::array<std::byte, cfg::kMaxFilterIdBytes> filter_id2_{
      std::byte(68), std::byte(69), std::byte(70), std::byte(0)};
  static constexpr std::array<std::byte, cfg::kMaxFilterIdBytes> filter_id3_{
      std::byte(71), std::byte(72), std::byte(73), std::byte(0)};
  std::array<Filter, kMaxDrains> filters_ = {
      Filter(filter_id1_, rules1_),
      Filter(filter_id2_, rules2_),
      Filter(filter_id3_, rules3_),
  };

  // Drain Buffers
  std::array<std::byte, kMaxLogEntrySize> drain_buffer1_;
  std::array<std::byte, kMaxLogEntrySize> drain_buffer2_;
  std::array<std::byte, RpcLogDrain::kMinEntryBufferSize> small_buffer_;
  static constexpr uint32_t kIgnoreWriterErrorsDrainId = 1;
  static constexpr uint32_t kCloseWriterOnErrorDrainId = 2;
  static constexpr uint32_t kSmallBufferDrainId = 3;
  sync::Mutex shared_mutex_;
  std::array<RpcLogDrain, kMaxDrains> drains_{
      RpcLogDrain(kIgnoreWriterErrorsDrainId,
                  drain_buffer1_,
                  shared_mutex_,
                  RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors,
                  &filters_[0]),
      RpcLogDrain(kCloseWriterOnErrorDrainId,
                  drain_buffer2_,
                  shared_mutex_,
                  RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError,
                  &filters_[1]),
      RpcLogDrain(kSmallBufferDrainId,
                  small_buffer_,
                  shared_mutex_,
                  RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors,
                  &filters_[2]),
  };
};
struct TestLogEntry {
  log_tokenized::Metadata metadata = kSampleMetadata;
  int64_t timestamp = 0;
  uint32_t dropped = 0;
  ConstByteSpan tokenized_data = {};
};

// Unpacks a `LogEntry` proto buffer and compares it with the expected data.
void VerifyLogEntry(protobuf::Decoder& entry_decoder,
                    const TestLogEntry& expected_entry) {
  ConstByteSpan tokenized_data;
  if (!expected_entry.tokenized_data.empty()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 1u);  // message [tokenized]
    ASSERT_TRUE(entry_decoder.ReadBytes(&tokenized_data).ok());
    if (tokenized_data.size() != expected_entry.tokenized_data.size()) {
      PW_LOG_ERROR(
          "actual: '%s', expected: '%s'",
          reinterpret_cast<const char*>(tokenized_data.begin()),
          reinterpret_cast<const char*>(expected_entry.tokenized_data.begin()));
    }
    EXPECT_EQ(tokenized_data.size(), expected_entry.tokenized_data.size());
    EXPECT_EQ(std::memcmp(tokenized_data.begin(),
                          expected_entry.tokenized_data.begin(),
                          expected_entry.tokenized_data.size()),
              0);
  }
  if (expected_entry.metadata.level()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 2u);  // line_level
    uint32_t line_level;
    ASSERT_TRUE(entry_decoder.ReadUint32(&line_level).ok());
    EXPECT_EQ(expected_entry.metadata.level(),
              line_level & PW_LOG_LEVEL_BITMASK);
    EXPECT_EQ(expected_entry.metadata.line_number(),
              (line_level & ~PW_LOG_LEVEL_BITMASK) >> PW_LOG_LEVEL_BITS);
  }
  if (expected_entry.metadata.flags()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 3u);  // flags
    uint32_t flags;
    ASSERT_TRUE(entry_decoder.ReadUint32(&flags).ok());
    EXPECT_EQ(expected_entry.metadata.flags(), flags);
  }
  if (expected_entry.timestamp) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_TRUE(entry_decoder.FieldNumber() == 4u       // timestamp
                || entry_decoder.FieldNumber() == 5u);  // time_since_last_entry
    int64_t timestamp;
    ASSERT_TRUE(entry_decoder.ReadInt64(&timestamp).ok());
    EXPECT_EQ(expected_entry.timestamp, timestamp);
  }
  if (expected_entry.dropped) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 6u);  // dropped
    uint32_t dropped;
    ASSERT_TRUE(entry_decoder.ReadUint32(&dropped).ok());
    EXPECT_EQ(expected_entry.dropped, dropped);
  }
  if (expected_entry.metadata.module()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 7u);  // module_name
    const Result<uint32_t> module =
        protobuf::DecodeBytesToUint32(entry_decoder);
    ASSERT_EQ(module.status(), OkStatus());
    EXPECT_EQ(expected_entry.metadata.module(), module.value());
  }
}

// Verifies a stream of log entries, returning the total count found.
size_t VerifyLogEntries(protobuf::Decoder& entries_decoder,
                        Vector<TestLogEntry>& expected_entries_stack) {
  size_t entries_found = 0;
  while (entries_decoder.Next().ok()) {
    ConstByteSpan entry;
    EXPECT_TRUE(entries_decoder.ReadBytes(&entry).ok());
    protobuf::Decoder entry_decoder(entry);
    if (expected_entries_stack.empty()) {
      break;
    }
    VerifyLogEntry(entry_decoder, expected_entries_stack.back());
    expected_entries_stack.pop_back();
    ++entries_found;
  }
  return entries_found;
}

size_t CountLogEntries(protobuf::Decoder& entries_decoder) {
  size_t entries_found = 0;
  while (entries_decoder.Next().ok()) {
    ++entries_found;
  }
  return entries_found;
}

TEST_F(LogServiceTest, AssignWriter) {
  // Drains don't have writers.
  for (auto& drain : drain_map_.drains()) {
    EXPECT_EQ(drain.Flush(), Status::Unavailable());
  }

  // Create context directed to drain with ID 1.
  RpcLogDrain& active_drain = drains_[0];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_, &filter_map_);
  context.set_channel_id(drain_channel_id);

  // Call RPC, which sets the drain's writer.
  context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());

  // Other drains are still missing writers.
  for (auto& drain : drain_map_.drains()) {
    if (drain.channel_id() != drain_channel_id) {
      EXPECT_EQ(drain.Flush(), Status::Unavailable());
    }
  }

  // Calling an ongoing log stream must not change the active drain's
  // writer, and the second writer must not get any responses.
  LOG_SERVICE_METHOD_CONTEXT second_call_context(drain_map_, &filter_map_);
  second_call_context.set_channel_id(drain_channel_id);
  second_call_context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());
  ASSERT_TRUE(second_call_context.done());
  EXPECT_EQ(second_call_context.responses().size(), 0u);

  // Setting a new writer on a closed stream is allowed.
  ASSERT_EQ(active_drain.Close(), OkStatus());
  LOG_SERVICE_METHOD_CONTEXT third_call_context(drain_map_, &filter_map_);
  third_call_context.set_channel_id(drain_channel_id);
  third_call_context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());
  ASSERT_FALSE(third_call_context.done());
  EXPECT_EQ(third_call_context.responses().size(), 0u);
  EXPECT_EQ(active_drain.Close(), OkStatus());
}

TEST_F(LogServiceTest, StartAndEndStream) {
  RpcLogDrain& active_drain = drains_[2];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_, &filter_map_);
  context.set_channel_id(drain_channel_id);

  // Add log entries.
  const size_t total_entries = 10;
  AddLogEntries(total_entries, kMessage, kSampleMetadata, kSampleTimestamp);

  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());

  // Not done until the stream is finished.
  ASSERT_FALSE(context.done());
  active_drain.Close();
  ASSERT_TRUE(context.done());

  EXPECT_EQ(context.status(), OkStatus());
  // There is at least 1 response with multiple log entries packed.
  EXPECT_GE(context.responses().size(), 1u);

  // Verify data in responses.
  Vector<TestLogEntry, total_entries> message_stack;
  for (size_t i = 0; i < total_entries; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }
  size_t entries_found = 0;
  for (auto& response : context.responses()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  EXPECT_EQ(entries_found, total_entries);
}

TEST_F(LogServiceTest, HandleDropped) {
  RpcLogDrain& active_drain = drains_[0];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_, &filter_map_);
  context.set_channel_id(drain_channel_id);

  // Add log entries.
  const size_t total_entries = 5;
  const uint32_t total_drop_count = 2;
  AddLogEntries(total_entries, kMessage, kSampleMetadata, kSampleTimestamp);
  multisink_.HandleDropped(total_drop_count);

  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());
  active_drain.Close();
  ASSERT_EQ(context.status(), OkStatus());
  // There is at least 1 response with multiple log entries packed.
  ASSERT_GE(context.responses().size(), 1u);

  // Add create expected messages in a stack to match the order they arrive in.
  Vector<TestLogEntry, total_entries + 1> message_stack;
  message_stack.push_back(
      {.metadata = kDropMessageMetadata, .dropped = total_drop_count});
  for (size_t i = 0; i < total_entries; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }

  // Verify data in responses.
  size_t entries_found = 0;
  for (auto& response : context.responses()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  // Expect an extra message with the drop count.
  EXPECT_EQ(entries_found, total_entries + 1);
}

TEST_F(LogServiceTest, HandleSmallBuffer) {
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_, &filter_map_);
  context.set_channel_id(kSmallBufferDrainId);
  auto small_buffer_drain =
      drain_map_.GetDrainFromChannelId(kSmallBufferDrainId);
  ASSERT_TRUE(small_buffer_drain.ok());

  // Add log entries.
  const size_t total_entries = 5;
  const uint32_t total_drop_count = total_entries;
  AddLogEntries(total_entries, kLongMessage, kSampleMetadata, kSampleTimestamp);
  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(small_buffer_drain.value()->Flush(), OkStatus());
  EXPECT_EQ(small_buffer_drain.value()->Close(), OkStatus());
  ASSERT_EQ(context.status(), OkStatus());
  ASSERT_GE(context.responses().size(), 1u);

  Vector<TestLogEntry, total_entries + 1> message_stack;
  message_stack.push_back(
      {.metadata = kDropMessageMetadata, .dropped = total_drop_count});

  // Verify data in responses.
  size_t entries_found = 0;
  for (auto& response : context.responses()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  // No messages fit the buffer, expect a drop message.
  EXPECT_EQ(entries_found, 1u);
}

TEST_F(LogServiceTest, FlushDrainWithoutMultisink) {
  auto& detached_drain = drains_[0];
  multisink_.DetachDrain(detached_drain);
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_, &filter_map_);
  context.set_channel_id(detached_drain.channel_id());

  // Add log entries.
  const size_t total_entries = 5;
  AddLogEntries(total_entries, kMessage, kSampleMetadata, kSampleTimestamp);
  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(detached_drain.Close(), OkStatus());
  ASSERT_EQ(context.status(), OkStatus());
  EXPECT_EQ(context.responses().size(), 0u);
}

TEST_F(LogServiceTest, LargeLogEntry) {
  const TestLogEntry expected_entry{
      .metadata =
          log_tokenized::Metadata::Set<PW_LOG_LEVEL_WARN,
                                       (1 << PW_LOG_TOKENIZED_MODULE_BITS) - 1,
                                       (1 << PW_LOG_TOKENIZED_FLAG_BITS) - 1,
                                       (1 << PW_LOG_TOKENIZED_LINE_BITS) - 1>(),
      .timestamp = std::numeric_limits<int64_t>::max(),
      .tokenized_data = std::as_bytes(std::span(kMessage)),
  };

  // Add entry to multisink.
  log::LogEntry::MemoryEncoder encoder(entry_encode_buffer_);
  ASSERT_EQ(encoder.WriteMessage(expected_entry.tokenized_data), OkStatus());
  ASSERT_EQ(encoder.WriteLineLevel(
                (expected_entry.metadata.level() & PW_LOG_LEVEL_BITMASK) |
                ((expected_entry.metadata.line_number() << PW_LOG_LEVEL_BITS) &
                 ~PW_LOG_LEVEL_BITMASK)),
            OkStatus());
  ASSERT_EQ(encoder.WriteFlags(expected_entry.metadata.flags()), OkStatus());
  ASSERT_EQ(encoder.WriteTimestamp(expected_entry.timestamp), OkStatus());
  const uint32_t little_endian_module = bytes::ConvertOrderTo(
      std::endian::little, expected_entry.metadata.module());
  ASSERT_EQ(
      encoder.WriteModule(std::as_bytes(std::span(&little_endian_module, 1))),
      OkStatus());
  ASSERT_EQ(encoder.status(), OkStatus());
  multisink_.HandleEntry(encoder);

  // Start log stream.
  RpcLogDrain& active_drain = drains_[0];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_, &filter_map_);
  context.set_channel_id(drain_channel_id);
  context.call(rpc_request_buffer);
  ASSERT_EQ(active_drain.Flush(), OkStatus());
  active_drain.Close();
  ASSERT_EQ(context.status(), OkStatus());
  ASSERT_EQ(context.responses().size(), 1u);

  // Verify message.
  protobuf::Decoder entries_decoder(context.responses()[0]);
  ASSERT_TRUE(entries_decoder.Next().ok());
  ConstByteSpan entry;
  EXPECT_TRUE(entries_decoder.ReadBytes(&entry).ok());
  protobuf::Decoder entry_decoder(entry);
  VerifyLogEntry(entry_decoder, expected_entry);
}

TEST_F(LogServiceTest, InterruptedLogStreamSendsDropCount) {
  const uint32_t drain_channel_id = kCloseWriterOnErrorDrainId;
  auto drain = drain_map_.GetDrainFromChannelId(drain_channel_id);
  ASSERT_TRUE(drain.ok());

  LogService log_service(drain_map_, &filter_map_);
  const size_t output_buffer_size = 128;
  const size_t max_packets = 10;
  rpc::RawFakeChannelOutput<10, output_buffer_size, 512> output;
  rpc::Channel channel(rpc::Channel::Create<drain_channel_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Add as many entries needed to have multiple packets send.
  StatusWithSize status =
      AddLogEntry(kMessage, kSampleMetadata, kSampleTimestamp);
  ASSERT_TRUE(status.ok());

  // In reality less than output_buffer_size is given as a buffer, since some
  // bytes are used for the RPC framing.
  const uint32_t max_messages_per_response = output_buffer_size / status.size();
  // Send less packets than the max to avoid crashes.
  const uint32_t packets_sent = max_packets / 2;
  const size_t total_entries = packets_sent * max_messages_per_response;
  const size_t max_entries = 50;
  // Check we can test all these entries.
  ASSERT_GE(max_entries, total_entries);
  AddLogEntries(total_entries - 1, kMessage, kSampleMetadata, kSampleTimestamp);

  // Interrupt log stream with an error.
  const uint32_t successful_packets_sent = packets_sent / 2;
  output.set_send_status(Status::Unavailable(), successful_packets_sent);

  // Request logs.
  rpc::RawServerWriter writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  // This drain closes on errors.
  EXPECT_EQ(drain.value()->Flush(), Status::Aborted());
  EXPECT_TRUE(output.done());

  // Make sure not all packets were sent.
  ASSERT_EQ(output.payloads<Logs::Listen>().size(), successful_packets_sent);

  // Verify data in responses.
  Vector<TestLogEntry, max_entries> message_stack;
  for (size_t i = 0; i < total_entries; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }
  size_t entries_found = 0;
  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }

  // Verify that not all the entries were sent.
  EXPECT_LE(entries_found, total_entries);

  // Reset channel output and resume log stream with a new writer.
  output.clear();
  writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  EXPECT_EQ(drain.value()->Flush(), OkStatus());

  // Add expected messages to the stack in the reverse order they are received.
  message_stack.clear();
  // One full packet was dropped. Since all messages are the same length, there
  // are entries_found / successful_packets_sent per packet.
  const uint32_t total_drop_count = entries_found / successful_packets_sent;
  const uint32_t remaining_entries = total_entries - total_drop_count;
  for (size_t i = 0; i < remaining_entries; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }
  message_stack.push_back(
      {.metadata = kDropMessageMetadata, .dropped = total_drop_count});

  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  // All entries are accounted for, including the drop message.
  EXPECT_EQ(entries_found, remaining_entries + 1);
}

TEST_F(LogServiceTest, InterruptedLogStreamIgnoresErrors) {
  const uint32_t drain_channel_id = kIgnoreWriterErrorsDrainId;
  auto drain = drain_map_.GetDrainFromChannelId(drain_channel_id);
  ASSERT_TRUE(drain.ok());

  LogService log_service(drain_map_, &filter_map_);
  const size_t output_buffer_size = 128;
  const size_t max_packets = 20;
  rpc::RawFakeChannelOutput<max_packets, output_buffer_size, 512> output;
  rpc::Channel channel(rpc::Channel::Create<drain_channel_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Add as many entries needed to have multiple packets send.
  StatusWithSize status =
      AddLogEntry(kMessage, kSampleMetadata, kSampleTimestamp);
  ASSERT_TRUE(status.ok());

  // In reality less than output_buffer_size is given as a buffer, since some
  // bytes are used for the RPC framing.
  const uint32_t max_messages_per_response = output_buffer_size / status.size();
  // Send less packets than the max to avoid crashes.
  const uint32_t packets_sent = 4;
  const size_t total_entries = packets_sent * max_messages_per_response;
  const size_t max_entries = 50;
  // Check we can test all these entries.q
  ASSERT_GE(max_entries, total_entries);
  AddLogEntries(total_entries - 1, kMessage, kSampleMetadata, kSampleTimestamp);

  // Interrupt log stream with an error.
  const uint32_t error_on_packet_count = packets_sent / 2;
  output.set_send_status(Status::Unavailable(), error_on_packet_count);

  // Request logs.
  rpc::RawServerWriter writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  // This drain ignores errors.
  EXPECT_EQ(drain.value()->Flush(), OkStatus());
  EXPECT_FALSE(output.done());

  // Make sure some packets were sent.
  ASSERT_GE(output.payloads<Logs::Listen>().size(), 0u);

  // Verify that not all the entries were sent.
  size_t entries_found = 0;
  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += CountLogEntries(entry_decoder);
  }
  EXPECT_LE(entries_found, total_entries);

  // Verify that all messages were sent and the drop count messageis ignored.
  const uint32_t total_drop_count = total_entries - entries_found + 1;
  Vector<TestLogEntry, max_entries> message_stack;
  for (size_t i = 0; i < total_drop_count; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }

  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    VerifyLogEntries(entry_decoder, message_stack);
  }

  // More calls to flush with errors will not affect this stubborn drain.
  const size_t previous_stream_packet_count =
      output.payloads<Logs::Listen>().size();
  output.set_send_status(Status::Unavailable());
  EXPECT_EQ(drain.value()->Flush(), OkStatus());
  EXPECT_FALSE(output.done());
  ASSERT_EQ(output.payloads<Logs::Listen>().size(),
            previous_stream_packet_count);

  output.clear();
  EXPECT_EQ(drain.value()->Close(), OkStatus());
  EXPECT_TRUE(output.done());
}

TEST_F(LogServiceTest, GetFilterIds) {
  PW_RAW_TEST_METHOD_CONTEXT(LogService, ListFilterIds, 1, 128)
  context(drain_map_, &filter_map_);
  context.call({});
  ASSERT_TRUE(context.done());
  ASSERT_EQ(context.responses().size(), 1u);
  protobuf::Decoder decoder(context.responses()[0]);

  for (const auto& filter : filter_map_.filters()) {
    ASSERT_EQ(decoder.Next(), OkStatus());
    ASSERT_EQ(decoder.FieldNumber(), 1u);  // filter_id
    ConstByteSpan filter_id;
    ASSERT_EQ(decoder.ReadBytes(&filter_id), OkStatus());
    ASSERT_EQ(filter_id.size(), filter.id().size());
    EXPECT_EQ(
        std::memcmp(filter_id.data(), filter.id().data(), filter_id.size()), 0);
  }
  EXPECT_FALSE(decoder.Next().ok());

  // No IDs reported when none registered in the filter map.
  PW_RAW_TEST_METHOD_CONTEXT(LogService, ListFilterIds, 1, 128)
  no_filter_context(drain_map_, nullptr);
  no_filter_context.call({});
  ASSERT_TRUE(no_filter_context.done());
  ASSERT_EQ(no_filter_context.responses().size(), 1u);
  protobuf::Decoder no_filter_decoder(no_filter_context.responses()[0]);
  uint32_t filter_count = 0;
  while (no_filter_decoder.Next().ok()) {
    EXPECT_EQ(no_filter_decoder.FieldNumber(), 1u);  // filter_id
    ++filter_count;
  }
  EXPECT_EQ(filter_count, 0u);
}

Status EncodeFilterRule(const Filter::Rule& rule,
                        log::FilterRule::StreamEncoder& encoder) {
  PW_TRY(
      encoder.WriteLevelGreaterThanOrEqual(rule.level_greater_than_or_equal));
  PW_TRY(encoder.WriteModuleEquals(rule.module_equals));
  PW_TRY(encoder.WriteAnyFlagsSet(rule.any_flags_set));
  return encoder.WriteAction(static_cast<log::FilterRule::Action>(rule.action));
}

Status EncodeFilter(const Filter& filter, log::Filter::StreamEncoder& encoder) {
  for (auto& rule : filter.rules()) {
    log::FilterRule::StreamEncoder rule_encoder = encoder.GetRuleEncoder();
    PW_TRY(EncodeFilterRule(rule, rule_encoder));
  }
  return OkStatus();
}

Result<ConstByteSpan> EncodeFilterRequest(const Filter& filter,
                                          ByteSpan buffer) {
  stream::MemoryWriter writer(buffer);
  std::byte encode_buffer[256];
  protobuf::StreamEncoder encoder(writer, encode_buffer);
  PW_TRY(encoder.WriteBytes(
      static_cast<uint32_t>(log::SetFilterRequest::Fields::FILTER_ID),
      filter.id()));
  {
    log::Filter::StreamEncoder filter_encoder = encoder.GetNestedEncoder(
        static_cast<uint32_t>(log::SetFilterRequest::Fields::FILTER));
    PW_TRY(EncodeFilter(filter, filter_encoder));
  }  // Let the StreamEncoder destructor finalize the data.
  return ConstByteSpan(writer.data(), writer.bytes_written());
}

void VerifyRule(const Filter::Rule& rule, const Filter::Rule& expected_rule) {
  EXPECT_EQ(rule.level_greater_than_or_equal,
            expected_rule.level_greater_than_or_equal);
  EXPECT_EQ(rule.module_equals, expected_rule.module_equals);
  EXPECT_EQ(rule.any_flags_set, expected_rule.any_flags_set);
  EXPECT_EQ(rule.action, expected_rule.action);
}

TEST_F(LogServiceTest, SetFilterRules) {
  const std::array<Filter::Rule, 4> new_rules{{
      {
          .action = Filter::Rule::Action::kKeep,
          .level_greater_than_or_equal = log::FilterRule::Level::DEBUG_LEVEL,
          .any_flags_set = 0x0f,
          .module_equals{std::byte(123)},
      },
      {
          .action = Filter::Rule::Action::kInactive,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0xef,
          .module_equals{},
      },
      {
          .action = Filter::Rule::Action::kKeep,
          .level_greater_than_or_equal = log::FilterRule::Level::INFO_LEVEL,
          .any_flags_set = 0x1234,
          .module_equals{std::byte(99)},
      },
      {
          .action = Filter::Rule::Action::kDrop,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0,
          .module_equals{std::byte(4)},
      },
  }};
  const Filter new_filter(filters_[0].id(),
                          const_cast<std::array<Filter::Rule, 4>&>(new_rules));

  std::byte request_buffer[512];
  const auto request = EncodeFilterRequest(new_filter, request_buffer);
  ASSERT_EQ(request.status(), OkStatus());

  PW_RAW_TEST_METHOD_CONTEXT(LogService, SetFilter, 1, 128)
  context(drain_map_, &filter_map_);
  context.call(request.value());

  size_t i = 0;
  for (const auto& rule : filters_[0].rules()) {
    VerifyRule(rule, new_rules[i++]);
  }
}

TEST_F(LogServiceTest, SetFilterRulesWhenUsedByDrain) {
  const std::array<Filter::Rule, 4> new_filter_rules{{
      {
          .action = Filter::Rule::Action::kKeep,
          .level_greater_than_or_equal = log::FilterRule::Level::CRITICAL_LEVEL,
          .any_flags_set = 0xfd,
          .module_equals{std::byte(543)},
      },
      {
          .action = Filter::Rule::Action::kInactive,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0xca,
          .module_equals{},
      },
      {
          .action = Filter::Rule::Action::kKeep,
          .level_greater_than_or_equal = log::FilterRule::Level::INFO_LEVEL,
          .any_flags_set = 0xabcd,
          .module_equals{std::byte(9000)},
      },
      {
          .action = Filter::Rule::Action::kDrop,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0,
          .module_equals{std::byte(123)},
      },
  }};
  Filter& filter = filters_[0];
  const Filter new_filter(
      filter.id(), const_cast<std::array<Filter::Rule, 4>&>(new_filter_rules));

  // Add callback to drain.
  RpcLogDrain& drain = drains_[0];

  std::byte request_buffer[256];
  const auto request = EncodeFilterRequest(new_filter, request_buffer);
  ASSERT_EQ(request.status(), OkStatus());

  PW_RAW_TEST_METHOD_CONTEXT(LogService, SetFilter, 1, 128)
  context(drain_map_, &filter_map_);
  context.set_channel_id(drain.channel_id());
  context.call(request.value());

  size_t i = 0;
  for (const auto& rule : filter.rules()) {
    VerifyRule(rule, new_filter_rules[i++]);
  }

  // A request for logs without a filter should not modify the filter.
  PW_RAW_TEST_METHOD_CONTEXT(LogService, SetFilter, 1, 128)
  context_no_filter(drain_map_, &filter_map_);
  context_no_filter.set_channel_id(drain.channel_id());
  context_no_filter.call({});
  i = 0;
  for (const auto& rule : filter.rules()) {
    VerifyRule(rule, new_filter_rules[i++]);
  }

  // A new request for logs with a new filter updates filter.
  const std::array<Filter::Rule, 4> second_filter_rules{{
      {
          .action = Filter::Rule::Action::kKeep,
          .level_greater_than_or_equal = log::FilterRule::Level::DEBUG_LEVEL,
          .any_flags_set = 0xab,
          .module_equals{},
      },
      {
          .action = Filter::Rule::Action::kDrop,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0x11,
          .module_equals{std::byte(34)},
      },
      {
          .action = Filter::Rule::Action::kKeep,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0xef,
          .module_equals{std::byte(23)},
      },
      {
          .action = Filter::Rule::Action::kDrop,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0x0f,
          .module_equals{},
      },
  }};
  const Filter second_filter(
      filter.id(),
      const_cast<std::array<Filter::Rule, 4>&>(second_filter_rules));

  std::memset(request_buffer, 0, sizeof(request_buffer));
  const auto second_filter_request =
      EncodeFilterRequest(second_filter, request_buffer);
  ASSERT_EQ(second_filter_request.status(), OkStatus());
  PW_RAW_TEST_METHOD_CONTEXT(LogService, SetFilter, 1, 128)
  context_new_filter(drain_map_, &filter_map_);
  context_new_filter.set_channel_id(drain.channel_id());
  context_new_filter.call(second_filter_request.value());

  i = 0;
  for (const auto& rule : filter.rules()) {
    VerifyRule(rule, second_filter_rules[i++]);
  }
}

TEST_F(LogServiceTest, FilterLogs) {
  // Add a variety of logs.
  const uint32_t module = 0xcafe;
  const uint32_t flags = 0x02;
  const uint32_t line_number = 100;
  const auto debug_metadata = log_tokenized::Metadata::
      Set<PW_LOG_LEVEL_DEBUG, module, flags, line_number>();
  ASSERT_TRUE(AddLogEntry(kMessage, debug_metadata, kSampleTimestamp).ok());
  const auto info_metadata = log_tokenized::Metadata::
      Set<PW_LOG_LEVEL_INFO, module, flags, line_number>();
  ASSERT_TRUE(AddLogEntry(kMessage, info_metadata, kSampleTimestamp).ok());
  const auto warn_metadata = log_tokenized::Metadata::
      Set<PW_LOG_LEVEL_WARN, module, flags, line_number>();
  ASSERT_TRUE(AddLogEntry(kMessage, warn_metadata, kSampleTimestamp).ok());
  const auto error_metadata = log_tokenized::Metadata::
      Set<PW_LOG_LEVEL_ERROR, module, flags, line_number>();
  ASSERT_TRUE(AddLogEntry(kMessage, error_metadata, kSampleTimestamp).ok());
  const auto different_flags_metadata = log_tokenized::Metadata::
      Set<PW_LOG_LEVEL_ERROR, module, 0x01, line_number>();
  ASSERT_TRUE(
      AddLogEntry(kMessage, different_flags_metadata, kSampleTimestamp).ok());
  const auto different_module_metadata = log_tokenized::Metadata::
      Set<PW_LOG_LEVEL_ERROR, 0xabcd, flags, line_number>();
  ASSERT_TRUE(
      AddLogEntry(kMessage, different_module_metadata, kSampleTimestamp).ok());

  // Add messages to the stack in the reverse order they are sent.
  Vector<TestLogEntry, 6> message_stack;
  message_stack.push_back(
      {.metadata = error_metadata,
       .timestamp = kSampleTimestamp,
       .tokenized_data = std::as_bytes(std::span(std::string_view(kMessage)))});
  message_stack.push_back(
      {.metadata = warn_metadata,
       .timestamp = kSampleTimestamp,
       .tokenized_data = std::as_bytes(std::span(std::string_view(kMessage)))});
  message_stack.push_back(
      {.metadata = info_metadata,
       .timestamp = kSampleTimestamp,
       .tokenized_data = std::as_bytes(std::span(std::string_view(kMessage)))});

  // Create request with filter.
  const auto module_little_endian =
      bytes::CopyInOrder<uint32_t>(std::endian::little, module);
  const std::array<Filter::Rule, 2> rules{{
      {.action = Filter::Rule::Action::kKeep,
       .level_greater_than_or_equal = log::FilterRule::Level::INFO_LEVEL,
       .any_flags_set = flags,
       .module_equals{module_little_endian.begin(),
                      module_little_endian.end()}},
      {
          .action = Filter::Rule::Action::kDrop,
          .level_greater_than_or_equal = log::FilterRule::Level::ANY_LEVEL,
          .any_flags_set = 0,
          .module_equals{},
      },
  }};

  RpcLogDrain& drain = drains_[1];
  Filter& filter = filters_[1];
  const Filter new_filter(filter.id(),
                          const_cast<std::array<Filter::Rule, 2>&>(rules));

  // Set filter.
  std::byte request_buffer[256];
  const auto request = EncodeFilterRequest(new_filter, request_buffer);
  ASSERT_EQ(request.status(), OkStatus());
  PW_RAW_TEST_METHOD_CONTEXT(LogService, SetFilter, 1, 128)
  set_filter_context(drain_map_, &filter_map_);
  set_filter_context.set_channel_id(drain.channel_id());
  set_filter_context.call(request.value());

  // Request logs.
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_, &filter_map_);
  context.set_channel_id(drain.channel_id());
  context.call(request.value());
  ASSERT_EQ(drain.Flush(), OkStatus());

  size_t entries_found = 0;
  for (auto& response : context.responses()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  EXPECT_EQ(entries_found, 3u);
}

TEST_F(LogServiceTest, ReopenClosedLogStreamWithAcquiredBuffer) {
  const uint32_t drain_channel_id = kCloseWriterOnErrorDrainId;
  auto drain = drain_map_.GetDrainFromChannelId(drain_channel_id);
  ASSERT_TRUE(drain.ok());

  LogService log_service(drain_map_);
  rpc::RawFakeChannelOutput<10, 128, 512> output;
  rpc::Channel channel(rpc::Channel::Create<drain_channel_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Request logs.
  rpc::RawServerWriter writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  // This drain closes on errors.
  EXPECT_EQ(drain.value()->Flush(), OkStatus());

  // Request log stream with a new writer.
  writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  EXPECT_EQ(drain.value()->Flush(), OkStatus());
}

void VerifyFilterRule(protobuf::Decoder& decoder,
                      const Filter::Rule& expected_rule) {
  ASSERT_TRUE(decoder.Next().ok());
  ASSERT_EQ(decoder.FieldNumber(), 1u);  // level_greater_than_or_equal
  log::FilterRule::Level level_greater_than_or_equal;
  ASSERT_EQ(decoder.ReadUint32(
                reinterpret_cast<uint32_t*>(&level_greater_than_or_equal)),
            OkStatus());
  EXPECT_EQ(level_greater_than_or_equal,
            expected_rule.level_greater_than_or_equal);

  ASSERT_TRUE(decoder.Next().ok());
  ASSERT_EQ(decoder.FieldNumber(), 2u);  // module_equals
  ConstByteSpan module_equals;
  ASSERT_EQ(decoder.ReadBytes(&module_equals), OkStatus());
  ASSERT_EQ(module_equals.size(), expected_rule.module_equals.size());
  EXPECT_EQ(std::memcmp(module_equals.data(),
                        expected_rule.module_equals.data(),
                        module_equals.size()),
            0);

  ASSERT_TRUE(decoder.Next().ok());
  ASSERT_EQ(decoder.FieldNumber(), 3u);  // any_flags_set
  uint32_t any_flags_set;
  ASSERT_EQ(decoder.ReadUint32(&any_flags_set), OkStatus());
  EXPECT_EQ(any_flags_set, expected_rule.any_flags_set);

  ASSERT_TRUE(decoder.Next().ok());
  ASSERT_EQ(decoder.FieldNumber(), 4u);  // action
  Filter::Rule::Action action;
  ASSERT_EQ(decoder.ReadUint32(reinterpret_cast<uint32_t*>(&action)),
            OkStatus());
  EXPECT_EQ(action, expected_rule.action);
}

void VerifyFilterRules(protobuf::Decoder& decoder,
                       std::span<const Filter::Rule> expected_rules) {
  size_t rules_found = 0;
  while (decoder.Next().ok()) {
    ConstByteSpan rule;
    EXPECT_TRUE(decoder.ReadBytes(&rule).ok());
    protobuf::Decoder rule_decoder(rule);
    if (rules_found >= expected_rules.size()) {
      break;
    }
    VerifyFilterRule(rule_decoder, expected_rules[rules_found]);
    ++rules_found;
  }
  EXPECT_EQ(rules_found, expected_rules.size());
}

TEST_F(LogServiceTest, GetFilterRules) {
  PW_RAW_TEST_METHOD_CONTEXT(LogService, GetFilter, 1, 128)
  context(drain_map_, &filter_map_);

  std::byte request_buffer[64];
  log::GetFilterRequest::MemoryEncoder encoder(request_buffer);
  encoder.WriteFilterId(filter_id1_);
  const auto request = ConstByteSpan(encoder);
  context.call(request);
  ASSERT_TRUE(context.done());
  ASSERT_EQ(context.responses().size(), 1u);

  // Verify against empty rules.
  protobuf::Decoder decoder(context.responses()[0]);
  VerifyFilterRules(decoder, rules1_);

  // Partially populate rules.
  rules1_[0].action = Filter::Rule::Action::kKeep;
  rules1_[0].level_greater_than_or_equal = log::FilterRule::Level::DEBUG_LEVEL;
  rules1_[0].any_flags_set = 0xab;
  const std::array<std::byte, 2> module1{std::byte(123), std::byte(0xab)};
  rules1_[0].module_equals.assign(module1.begin(), module1.end());
  rules1_[1].action = Filter::Rule::Action::kDrop;
  rules1_[1].level_greater_than_or_equal = log::FilterRule::Level::ERROR_LEVEL;
  rules1_[1].any_flags_set = 0;

  PW_RAW_TEST_METHOD_CONTEXT(LogService, GetFilter, 1, 128)
  context2(drain_map_, &filter_map_);
  context2.call(request);
  ASSERT_EQ(context2.responses().size(), 1u);
  protobuf::Decoder decoder2(context2.responses()[0]);
  VerifyFilterRules(decoder2, rules1_);

  // Modify the rest of the filter rules.
  rules1_[2].action = Filter::Rule::Action::kKeep;
  rules1_[2].level_greater_than_or_equal = log::FilterRule::Level::FATAL_LEVEL;
  rules1_[2].any_flags_set = 0xcd;
  const std::array<std::byte, 2> module2{std::byte(1), std::byte(2)};
  rules1_[2].module_equals.assign(module2.begin(), module2.end());
  rules1_[3].action = Filter::Rule::Action::kInactive;

  PW_RAW_TEST_METHOD_CONTEXT(LogService, GetFilter, 1, 128)
  context3(drain_map_, &filter_map_);
  context3.call(request);
  ASSERT_EQ(context3.responses().size(), 1u);
  protobuf::Decoder decoder3(context3.responses()[0]);
  VerifyFilterRules(decoder3, rules1_);
}

}  // namespace
}  // namespace pw::log_rpc
