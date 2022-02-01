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

#include "pw_log_rpc/rpc_log_drain.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_bytes/span.h"
#include "pw_log/proto/log.pwpb.h"
#include "pw_log/proto_utils.h"
#include "pw_log_rpc/log_filter.h"
#include "pw_log_rpc/log_service.h"
#include "pw_log_rpc/rpc_log_drain_map.h"
#include "pw_log_rpc_private/test_utils.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_multisink/multisink.h"
#include "pw_protobuf/decoder.h"
#include "pw_protobuf/serialized_size.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/raw/fake_channel_output.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_status/status.h"
#include "pw_sync/mutex.h"

namespace pw::log_rpc {
namespace {
static constexpr size_t kBufferSize =
    RpcLogDrain::kMinEntrySizeWithoutPayload + 32;

// Verifies a stream of log entries and updates the total drop count found.
// expected_entries is expected to be in the same order that messages were
// added to the multisink.
void VerifyLogEntriesInCorrectOrder(
    protobuf::Decoder& entries_decoder,
    const Vector<TestLogEntry>& expected_entries,
    uint32_t expected_first_entry_sequence_id,
    uint32_t& drop_count_out) {
  size_t entries_found = 0;

  while (entries_decoder.Next().ok()) {
    if (static_cast<pw::log::LogEntries::Fields>(
            entries_decoder.FieldNumber()) ==
        log::LogEntries::Fields::ENTRIES) {
      ConstByteSpan entry;
      EXPECT_EQ(entries_decoder.ReadBytes(&entry), OkStatus());
      protobuf::Decoder entry_decoder(entry);
      if (expected_entries.empty()) {
        break;
      }

      ASSERT_LT(entries_found, expected_entries.size());

      // Keep track of entries and drops respective counts.
      uint32_t current_drop_count = 0;
      VerifyLogEntry(
          entry_decoder, expected_entries[entries_found], current_drop_count);
      drop_count_out += current_drop_count;
      if (current_drop_count == 0) {
        ++entries_found;
      }
    } else if (static_cast<pw::log::LogEntries::Fields>(
                   entries_decoder.FieldNumber()) ==
               log::LogEntries::Fields::FIRST_ENTRY_SEQUENCE_ID) {
      uint32_t first_entry_sequence_id = 0;
      EXPECT_EQ(entries_decoder.ReadUint32(&first_entry_sequence_id),
                OkStatus());
      EXPECT_EQ(expected_first_entry_sequence_id, first_entry_sequence_id);
    }
  }
}

TEST(RpcLogDrain, TryFlushDrainWithClosedWriter) {
  // Drain without a writer.
  const uint32_t drain_id = 1;
  std::array<std::byte, kBufferSize> buffer;
  sync::Mutex mutex;
  RpcLogDrain drain(
      drain_id,
      buffer,
      mutex,
      RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError,
      nullptr);
  EXPECT_EQ(drain.channel_id(), drain_id);

  std::byte encoding_buffer[128] = {};

  // Attach drain to a MultiSink.
  std::array<std::byte, kBufferSize * 2> multisink_buffer;
  multisink::MultiSink multisink(multisink_buffer);
  multisink.AttachDrain(drain);
  EXPECT_EQ(drain.Flush(encoding_buffer), Status::Unavailable());

  rpc::RawServerWriter writer;
  ASSERT_FALSE(writer.active());
  EXPECT_EQ(drain.Open(writer), Status::FailedPrecondition());
  EXPECT_EQ(drain.Flush(encoding_buffer), Status::Unavailable());
}

TEST(RpcLogDrainMap, GetDrainsByIdFromDrainMap) {
  static constexpr size_t kMaxDrains = 3;
  sync::Mutex mutex;
  std::array<std::array<std::byte, kBufferSize>, kMaxDrains> buffers;
  std::array<RpcLogDrain, kMaxDrains> drains{
      RpcLogDrain(0,
                  buffers[0],
                  mutex,
                  RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError,
                  nullptr),
      RpcLogDrain(1,
                  buffers[1],
                  mutex,
                  RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError,
                  nullptr),
      RpcLogDrain(2,
                  buffers[2],
                  mutex,
                  RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors,
                  nullptr),
  };

  RpcLogDrainMap drain_map(drains);
  for (uint32_t channel_id = 0; channel_id < kMaxDrains; ++channel_id) {
    auto drain_result = drain_map.GetDrainFromChannelId(channel_id);
    ASSERT_TRUE(drain_result.ok());
    EXPECT_EQ(drain_result.value(), &drains[channel_id]);
  }
  const std::span<RpcLogDrain> mapped_drains = drain_map.drains();
  ASSERT_EQ(mapped_drains.size(), kMaxDrains);
  for (uint32_t channel_id = 0; channel_id < kMaxDrains; ++channel_id) {
    EXPECT_EQ(&mapped_drains[channel_id], &drains[channel_id]);
  }
}

TEST(RpcLogDrain, FlushingDrainWithOpenWriter) {
  const uint32_t drain_id = 1;
  std::array<std::byte, kBufferSize> buffer;
  sync::Mutex mutex;
  std::array<RpcLogDrain, 1> drains{
      RpcLogDrain(drain_id,
                  buffer,
                  mutex,
                  RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError,
                  nullptr),
  };
  RpcLogDrainMap drain_map(drains);
  LogService log_service(drain_map);

  std::byte encoding_buffer[128] = {};

  rpc::RawFakeChannelOutput<3> output;
  rpc::Channel channel(rpc::Channel::Create<drain_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Attach drain to a MultiSink.
  RpcLogDrain& drain = drains[0];
  std::array<std::byte, kBufferSize * 2> multisink_buffer;
  multisink::MultiSink multisink(multisink_buffer);
  multisink.AttachDrain(drain);
  EXPECT_EQ(drain.Flush(encoding_buffer), Status::Unavailable());

  rpc::RawServerWriter writer =
      rpc::RawServerWriter::Open<log::pw_rpc::raw::Logs::Listen>(
          server, drain_id, log_service);
  ASSERT_TRUE(writer.active());
  EXPECT_EQ(drain.Open(writer), OkStatus());
  EXPECT_EQ(drain.Flush(encoding_buffer), OkStatus());
  // Can call multliple times until closed on error.
  EXPECT_EQ(drain.Flush(encoding_buffer), OkStatus());
  EXPECT_EQ(drain.Close(), OkStatus());
  rpc::RawServerWriter& writer_ref = writer;
  ASSERT_FALSE(writer_ref.active());
  EXPECT_EQ(drain.Flush(encoding_buffer), Status::Unavailable());
}

TEST(RpcLogDrain, TryReopenOpenedDrain) {
  const uint32_t drain_id = 1;
  std::array<std::byte, kBufferSize> buffer;
  sync::Mutex mutex;
  std::array<RpcLogDrain, 1> drains{
      RpcLogDrain(drain_id,
                  buffer,
                  mutex,
                  RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError,
                  nullptr),
  };
  RpcLogDrainMap drain_map(drains);
  LogService log_service(drain_map);

  rpc::RawFakeChannelOutput<1> output;
  rpc::Channel channel(rpc::Channel::Create<drain_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Open Drain and try to open with a new writer.
  rpc::RawServerWriter writer =
      rpc::RawServerWriter::Open<log::pw_rpc::raw::Logs::Listen>(
          server, drain_id, log_service);
  ASSERT_TRUE(writer.active());
  RpcLogDrain& drain = drains[0];
  EXPECT_EQ(drain.Open(writer), OkStatus());
  rpc::RawServerWriter second_writer =
      rpc::RawServerWriter::Open<log::pw_rpc::raw::Logs::Listen>(
          server, drain_id, log_service);
  ASSERT_FALSE(writer.active());
  ASSERT_TRUE(second_writer.active());
  EXPECT_EQ(drain.Open(second_writer), OkStatus());
}

class TrickleTest : public ::testing::Test {
 protected:
  TrickleTest()
      : log_message_encode_buffer_(),
        drain_encode_buffer_(),
        channel_encode_buffer_(),
        mutex_(),
        drains_{
            RpcLogDrain(
                kDrainChannelId,
                drain_encode_buffer_,
                mutex_,
                RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError,
                nullptr),
        },
        multisink_buffer_(),
        multisink_(multisink_buffer_),
        drain_map_(drains_),
        log_service_(drain_map_),
        output_(),
        channel_(rpc::Channel::Create<kDrainChannelId>(&output_)),
        server_(std::span(&channel_, 1)) {}

  TestLogEntry BasicLog(std::string_view message) {
    constexpr log_tokenized::Metadata kSampleMetadata =
        log_tokenized::Metadata::Set<PW_LOG_LEVEL_INFO, 123, 0x03, __LINE__>();
    return {.metadata = kSampleMetadata,
            .timestamp = 123,
            .dropped = 0,
            .tokenized_data = std::as_bytes(std::span(message))};
  }

  void AttachDrain() { multisink_.AttachDrain(drains_[0]); }
  void OpenWriter() {
    writer_ = rpc::RawServerWriter::Open<log::pw_rpc::raw::Logs::Listen>(
        server_, kDrainChannelId, log_service_);
  }

  void AddLogEntry(const TestLogEntry& entry) {
    Result<ConstByteSpan> encoded_log_result =
        log::EncodeTokenizedLog(entry.metadata,
                                entry.tokenized_data,
                                entry.timestamp,
                                log_message_encode_buffer_);
    ASSERT_EQ(encoded_log_result.status(), OkStatus());
    EXPECT_LE(encoded_log_result.value().size(), kMaxMessageSize);
    multisink_.HandleEntry(encoded_log_result.value());
  }

  // VerifyLogEntriesInCorrectOrder() expects logs to be in the opposite
  // direction compared to when they were added to the multisink.
  void AddLogEntries(const Vector<TestLogEntry>& entries) {
    for (const TestLogEntry& entry : entries) {
      AddLogEntry(entry);
    }
  }

  static constexpr uint32_t kDrainChannelId = 1;
  static constexpr size_t kMaxMessageSize = 60;
  static constexpr size_t kDrainEncodeBufferSize =
      RpcLogDrain::kMinEntrySizeWithoutPayload + kMaxMessageSize;
  static constexpr size_t kChannelEncodeBufferSize = kDrainEncodeBufferSize * 2;
  std::array<std::byte, kMaxMessageSize> log_message_encode_buffer_;
  std::array<std::byte, kDrainEncodeBufferSize> drain_encode_buffer_;
  // Make actual encode buffer slightly smaller to account for RPC overhead.
  std::array<std::byte, kChannelEncodeBufferSize - 8> channel_encode_buffer_;
  sync::Mutex mutex_;
  std::array<RpcLogDrain, 1> drains_;

  std::array<std::byte, kDrainEncodeBufferSize * 12> multisink_buffer_;
  multisink::MultiSink multisink_;

  RpcLogDrainMap drain_map_;
  LogService log_service_;

  // TODO(amontanez): Why do we need 4 packets? Three should work, but seemingly
  // on destruction a 14-byte payload is sent out, forcing us to use max
  // expected packet count plus one.
  rpc::RawFakeChannelOutput<4, kDrainEncodeBufferSize * 6> output_;
  rpc::Channel channel_;
  rpc::Server server_;
  rpc::RawServerWriter writer_;
};

TEST_F(TrickleTest, EntriesAreFlushedToSinglePayload) {
  AttachDrain();
  OpenWriter();

  Vector<TestLogEntry, 3> kExpectedEntries{
      BasicLog(":D"), BasicLog("A useful log"), BasicLog("blink")};
  AddLogEntries(kExpectedEntries);

  ASSERT_TRUE(writer_.active());
  EXPECT_EQ(drains_[0].Open(writer_), OkStatus());

  std::optional<chrono::SystemClock::duration> min_delay =
      drains_[0].Trickle(channel_encode_buffer_);
  EXPECT_EQ(min_delay.has_value(), false);

  rpc::PayloadsView payloads =
      output_.payloads<log::pw_rpc::raw::Logs::Listen>(kDrainChannelId);
  EXPECT_EQ(payloads.size(), 1u);

  uint32_t drop_count = 0;
  protobuf::Decoder payload_decoder(payloads[0]);
  payload_decoder.Reset(payloads[0]);
  VerifyLogEntriesInCorrectOrder(
      payload_decoder, kExpectedEntries, 0, drop_count);
  EXPECT_EQ(drop_count, 0u);
}

TEST_F(TrickleTest, ManyLogsOverflowToNextPayload) {
  AttachDrain();
  OpenWriter();

  Vector<TestLogEntry, 3> kFirstFlushedBundle{
      BasicLog("Use longer logs in this test"),
      BasicLog("My feet are cold"),
      BasicLog("I'm hungry, what's for dinner?")};
  Vector<TestLogEntry, 3> kSecondFlushedBundle{
      BasicLog("Add a few longer logs"),
      BasicLog("Eventually the logs will"),
      BasicLog("Overflow into another payload")};

  AddLogEntries(kFirstFlushedBundle);
  AddLogEntries(kSecondFlushedBundle);

  ASSERT_TRUE(writer_.active());
  EXPECT_EQ(drains_[0].Open(writer_), OkStatus());

  // A single flush should produce two payloads.
  std::optional<chrono::SystemClock::duration> min_delay =
      drains_[0].Trickle(channel_encode_buffer_);
  EXPECT_EQ(min_delay.has_value(), false);

  rpc::PayloadsView payloads =
      output_.payloads<log::pw_rpc::raw::Logs::Listen>(kDrainChannelId);
  ASSERT_EQ(payloads.size(), 2u);

  uint32_t drop_count = 0;
  protobuf::Decoder payload_decoder(payloads[0]);
  payload_decoder.Reset(payloads[0]);
  VerifyLogEntriesInCorrectOrder(
      payload_decoder, kFirstFlushedBundle, 0, drop_count);
  EXPECT_EQ(drop_count, 0u);

  payload_decoder.Reset(payloads[1]);
  VerifyLogEntriesInCorrectOrder(
      payload_decoder, kSecondFlushedBundle, 3, drop_count);
  EXPECT_EQ(drop_count, 0u);
}

TEST_F(TrickleTest, LimitedFlushOverflowsToNextPayload) {
  AttachDrain();
  OpenWriter();

  Vector<TestLogEntry, 3> kFirstFlushedBundle{
      BasicLog("Use longer logs in this test"),
      BasicLog("My feet are cold"),
      BasicLog("I'm hungry, what's for dinner?")};
  Vector<TestLogEntry, 3> kSecondFlushedBundle{
      BasicLog("Add a few longer logs"),
      BasicLog("Eventually the logs will"),
      BasicLog("Overflow into another payload")};

  AddLogEntries(kFirstFlushedBundle);

  // These logs will get pushed into the next payload due to overflowing max
  // payload size.
  AddLogEntries(kSecondFlushedBundle);

  ASSERT_TRUE(writer_.active());
  EXPECT_EQ(drains_[0].Open(writer_), OkStatus());
  drains_[0].set_max_bundles_per_trickle(1);

  // A single flush should produce two payloads.
  std::optional<chrono::SystemClock::duration> min_delay =
      drains_[0].Trickle(channel_encode_buffer_);
  EXPECT_EQ(min_delay.has_value(), true);
  EXPECT_EQ(min_delay.value(), chrono::SystemClock::duration::zero());

  rpc::PayloadsView first_flush_payloads =
      output_.payloads<log::pw_rpc::raw::Logs::Listen>(kDrainChannelId);
  ASSERT_EQ(first_flush_payloads.size(), 1u);
  uint32_t drop_count = 0;
  protobuf::Decoder payload_decoder(first_flush_payloads[0]);
  payload_decoder.Reset(first_flush_payloads[0]);
  VerifyLogEntriesInCorrectOrder(
      payload_decoder, kFirstFlushedBundle, 0, drop_count);

  // An additional flush should produce another payload.
  min_delay = drains_[0].Trickle(channel_encode_buffer_);
  EXPECT_EQ(min_delay.has_value(), false);
  drop_count = 0;
  rpc::PayloadsView second_flush_payloads =
      output_.payloads<log::pw_rpc::raw::Logs::Listen>(kDrainChannelId);
  ASSERT_EQ(second_flush_payloads.size(), 2u);
  payload_decoder.Reset(second_flush_payloads[1]);
  VerifyLogEntriesInCorrectOrder(
      payload_decoder, kSecondFlushedBundle, 3, drop_count);
  EXPECT_EQ(drop_count, 0u);
}

}  // namespace
}  // namespace pw::log_rpc
