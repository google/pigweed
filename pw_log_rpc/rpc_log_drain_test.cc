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
#include "pw_log_rpc/rpc_log_drain_map.h"
#include "pw_multisink/multisink.h"
#include "pw_status/status.h"
#include "pw_sync/mutex.h"

namespace pw::log_rpc {
namespace {
static constexpr size_t kBufferSize =
    RpcLogDrain::kMinEntrySizeWithoutPayload + 32;

TEST(RpcLogDrain, TryFlushDrainWithClosedWriter) {
  // Drain without a writer.
  const uint32_t drain_id = 1;
  std::array<std::byte, kBufferSize> buffer1;
  sync::Mutex mutex;
  RpcLogDrain drain(
      drain_id,
      buffer1,
      mutex,
      RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError);
  EXPECT_EQ(drain.channel_id(), drain_id);

  // Attach drain to a MultiSink.
  std::array<std::byte, kBufferSize * 2> multisink_buffer;
  multisink::MultiSink multisink(multisink_buffer);
  multisink.AttachDrain(drain);
  EXPECT_EQ(drain.Flush(), Status::Unavailable());

  rpc::RawServerWriter writer;
  ASSERT_FALSE(writer.open());
  EXPECT_EQ(drain.Open(writer), Status::FailedPrecondition());
  EXPECT_EQ(drain.Flush(), Status::Unavailable());
}

TEST(RpcLogDrainMap, GetDrainsByIdFromDrainMap) {
  static constexpr size_t kMaxDrains = 3;
  sync::Mutex mutex;
  std::array<std::array<std::byte, kBufferSize>, kMaxDrains> buffers;
  std::array<RpcLogDrain, kMaxDrains> drains{
      RpcLogDrain(
          0,
          buffers[0],
          mutex,
          RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError),
      RpcLogDrain(
          1,
          buffers[1],
          mutex,
          RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError),
      RpcLogDrain(2,
                  buffers[2],
                  mutex,
                  RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors),
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

// TODO(cachinchilla): add tests for passing an open RawServerWriter when there
// is a way to create an one manually.

}  // namespace
}  // namespace pw::log_rpc
