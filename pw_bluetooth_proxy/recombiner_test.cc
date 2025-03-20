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

#include "pw_bluetooth_proxy/internal/recombiner.h"

#include <cstdint>
#include <mutex>
#include <optional>

#include "pw_bluetooth_proxy/internal/locked_l2cap_channel.h"
#include "pw_bluetooth_proxy/proxy_host.h"
#include "pw_bluetooth_proxy_private/test_utils.h"
#include "pw_containers/to_array.h"
#include "pw_span/cast.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::bluetooth::proxy {
namespace {

using pw::containers::to_array;

class RecombinerTest : public ProxyHostTest {};

TEST_F(RecombinerTest, InactiveAtCreation) {
  Recombiner recombiner{Direction::kFromHost};

  EXPECT_FALSE(recombiner.IsActive());
}

TEST_F(RecombinerTest, Start) {
  ProxyHost proxy_{[]([[maybe_unused]] H4PacketWithHci&& packet) {},
                   []([[maybe_unused]] H4PacketWithH4&& packet) {},
                   0,
                   0};
  BasicL2capChannel channel = BuildBasicL2capChannel(proxy_, {});
  pw::sync::Mutex mutex;
  LockedL2capChannel locked_channel{channel, std::unique_lock(mutex)};

  Recombiner recombiner{Direction::kFromHost};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(locked_channel, 8u));

  EXPECT_TRUE(recombiner.IsActive());
  EXPECT_FALSE(recombiner.IsComplete());
}

TEST_F(RecombinerTest, GetLocalCid) {
  constexpr uint16_t kLocalCid = 0x20;
  ProxyHost proxy_{[]([[maybe_unused]] H4PacketWithHci&& packet) {},
                   []([[maybe_unused]] H4PacketWithH4&& packet) {},
                   0,
                   0};
  BasicL2capChannel channel =
      BuildBasicL2capChannel(proxy_, {.local_cid = kLocalCid});
  pw::sync::Mutex mutex;
  LockedL2capChannel locked_channel{channel, std::unique_lock(mutex)};

  Recombiner recombiner{Direction::kFromController};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(locked_channel, 8u));

  EXPECT_EQ(recombiner.local_cid(), kLocalCid);
}

TEST_F(RecombinerTest, EndWithChannel) {
  ProxyHost proxy_{[]([[maybe_unused]] H4PacketWithHci&& packet) {},
                   []([[maybe_unused]] H4PacketWithH4&& packet) {},
                   0,
                   0};
  BasicL2capChannel channel = BuildBasicL2capChannel(proxy_, {});
  pw::sync::Mutex mutex;
  std::optional<LockedL2capChannel> locked_channel{
      LockedL2capChannel{channel, std::unique_lock(mutex)}};

  Recombiner recombiner{Direction::kFromController};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(*locked_channel, 8u));

  EXPECT_TRUE(recombiner.IsActive());

  recombiner.EndRecombination(locked_channel);

  EXPECT_FALSE(recombiner.IsActive());
}

TEST_F(RecombinerTest, EndWithoutChannel) {
  ProxyHost proxy_{[]([[maybe_unused]] H4PacketWithHci&& packet) {},
                   []([[maybe_unused]] H4PacketWithH4&& packet) {},
                   0,
                   0};
  Recombiner recombiner{Direction::kFromController};

  {
    BasicL2capChannel channel = BuildBasicL2capChannel(proxy_, {});
    pw::sync::Mutex mutex;
    std::optional<LockedL2capChannel> locked_channel{
        LockedL2capChannel{channel, std::unique_lock(mutex)}};

    PW_TEST_EXPECT_OK(recombiner.StartRecombination(*locked_channel, 8u));
  }

  EXPECT_TRUE(recombiner.IsActive());

  std::optional<LockedL2capChannel> null_channel = std::nullopt;

  recombiner.EndRecombination(null_channel);

  EXPECT_FALSE(recombiner.IsActive());
}

TEST_F(RecombinerTest, WriteTakeEnd) {
  ProxyHost proxy_{[]([[maybe_unused]] H4PacketWithHci&& packet) {},
                   []([[maybe_unused]] H4PacketWithH4&& packet) {},
                   0,
                   0};
  Recombiner recombiner{Direction::kFromController};
  BasicL2capChannel channel = BuildBasicL2capChannel(proxy_, {});
  pw::sync::Mutex mutex;
  std::optional<LockedL2capChannel> locked_channel{
      LockedL2capChannel{channel, std::unique_lock(mutex)}};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(*locked_channel, 8u));

  static constexpr std::array<uint8_t, 8> kExpectedData = {
      0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  // Write first chunk
  PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
      locked_channel, to_array<uint8_t>({0x11, 0x22, 0x33, 0x44})));

  EXPECT_FALSE(recombiner.IsComplete());

  // Write second chunk
  PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
      locked_channel, to_array<uint8_t>({0x55, 0x66, 0x77, 0x88})));

  // Test combined result
  EXPECT_TRUE(recombiner.IsComplete());

  multibuf::MultiBuf mbuf = recombiner.TakeAndEnd(locked_channel);
  EXPECT_TRUE(mbuf.IsContiguous());

  EXPECT_FALSE(recombiner.IsActive());

  std::optional<ByteSpan> mbuf_span = mbuf.ContiguousSpan();
  EXPECT_TRUE(mbuf_span);

  pw::span<uint8_t> span = pw::span(
      reinterpret_cast<uint8_t*>(mbuf_span->data()), mbuf_span->size());

  EXPECT_TRUE(std::equal(
      span.begin(), span.end(), kExpectedData.begin(), kExpectedData.end()));
}

TEST_F(RecombinerTest, WriteCompleteWithoutChannel) {
  ProxyHost proxy_{[]([[maybe_unused]] H4PacketWithHci&& packet) {},
                   []([[maybe_unused]] H4PacketWithH4&& packet) {},
                   0,
                   0};
  Recombiner recombiner{Direction::kFromController};

  {
    BasicL2capChannel channel = BuildBasicL2capChannel(proxy_, {});
    pw::sync::Mutex mutex;
    std::optional<LockedL2capChannel> locked_channel{
        LockedL2capChannel{channel, std::unique_lock(mutex)}};

    PW_TEST_EXPECT_OK(recombiner.StartRecombination(*locked_channel, 8u));

    // Write first chunk
    PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
        locked_channel, to_array<uint8_t>({0x11, 0x22, 0x33, 0x44})));

    EXPECT_FALSE(recombiner.IsComplete());
  }

  std::optional<LockedL2capChannel> null_channel = std::nullopt;

  // Write second chunk
  PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
      null_channel, to_array<uint8_t>({0x55, 0x66, 0x77, 0x88})));

  EXPECT_TRUE(recombiner.IsComplete());

  // Typically client ends recombination at this point.
  recombiner.EndRecombination(null_channel);
  EXPECT_FALSE(recombiner.IsActive());
}

TEST_F(RecombinerTest, CannotOverwrite) {
  ProxyHost proxy_{[]([[maybe_unused]] H4PacketWithHci&& packet) {},
                   []([[maybe_unused]] H4PacketWithH4&& packet) {},
                   0,
                   0};
  Recombiner recombiner{Direction::kFromController};

  BasicL2capChannel channel = BuildBasicL2capChannel(proxy_, {});
  pw::sync::Mutex mutex;
  std::optional<LockedL2capChannel> locked_channel{
      LockedL2capChannel{channel, std::unique_lock(mutex)}};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(*locked_channel, 8u));

  // Write first chunk
  PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
      locked_channel, to_array<uint8_t>({0x11, 0x22, 0x33, 0x44})));

  EXPECT_FALSE(recombiner.IsComplete());

  // Try to write too large a second chunk.
  EXPECT_EQ(
      recombiner.RecombineFragment(
          locked_channel, to_array<uint8_t>({0x55, 0x66, 0x77, 0x88, 0x99})),
      pw::Status::ResourceExhausted());

  // Should still not be complete.
  EXPECT_FALSE(recombiner.IsComplete());

  // Client ends recombination at this point.
  recombiner.EndRecombination(locked_channel);
  EXPECT_FALSE(recombiner.IsActive());
}

}  // namespace
}  // namespace pw::bluetooth::proxy
