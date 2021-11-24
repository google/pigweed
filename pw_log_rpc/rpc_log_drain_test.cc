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

#include "gtest/gtest.h"
#include "pw_log_rpc/log_filter.h"
#include "pw_log_rpc/log_service.h"
#include "pw_log_rpc/rpc_log_drain_map.h"
#include "pw_multisink/multisink.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/raw/fake_channel_output.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_status/status.h"
#include "pw_sync/mutex.h"

namespace pw::log_rpc {
namespace {
static constexpr size_t kBufferSize =
    RpcLogDrain::kMinEntrySizeWithoutPayload + 32;

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

  // Attach drain to a MultiSink.
  std::array<std::byte, kBufferSize * 2> multisink_buffer;
  multisink::MultiSink multisink(multisink_buffer);
  multisink.AttachDrain(drain);
  EXPECT_EQ(drain.Flush(), Status::Unavailable());

  rpc::RawServerWriter writer;
  ASSERT_FALSE(writer.active());
  EXPECT_EQ(drain.Open(writer), Status::FailedPrecondition());
  EXPECT_EQ(drain.Flush(), Status::Unavailable());
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
  LogService log_service(drain_map, nullptr);

  rpc::RawFakeChannelOutput<3, 128> output;
  rpc::Channel channel(rpc::Channel::Create<drain_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Attach drain to a MultiSink.
  RpcLogDrain& drain = drains[0];
  std::array<std::byte, kBufferSize * 2> multisink_buffer;
  multisink::MultiSink multisink(multisink_buffer);
  multisink.AttachDrain(drain);
  EXPECT_EQ(drain.Flush(), Status::Unavailable());

  rpc::RawServerWriter writer =
      rpc::RawServerWriter::Open<log::pw_rpc::raw::Logs::Listen>(
          server, drain_id, log_service);
  ASSERT_TRUE(writer.active());
  EXPECT_EQ(drain.Open(writer), OkStatus());
  EXPECT_EQ(drain.Flush(), OkStatus());
  // Can call multliple times until closed on error.
  EXPECT_EQ(drain.Flush(), OkStatus());
  EXPECT_EQ(drain.Close(), OkStatus());
  rpc::RawServerWriter& writer_ref = writer;
  ASSERT_FALSE(writer_ref.active());
  EXPECT_EQ(drain.Flush(), Status::Unavailable());
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
  LogService log_service(drain_map, nullptr);

  rpc::RawFakeChannelOutput<1, 128> output;
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

}  // namespace
}  // namespace pw::log_rpc
