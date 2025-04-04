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

#include "pw_bluetooth_sapphire/internal/host/l2cap/basic_mode_tx_engine.h"

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_tx_channel.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_unit_test/framework.h"

namespace bt::l2cap::internal {
namespace {

constexpr ChannelId kTestChannelId = 0x0001;

TEST(BasicModeTxEngineTest, ProcessSduTransmitsMinimalSizedSdu) {
  ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  FakeTxChannel channel;
  channel.HandleSendFrame([&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  });

  constexpr size_t kMtu = 10;
  const StaticByteBuffer payload(1);
  channel.QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
  BasicModeTxEngine engine(kTestChannelId, kMtu, channel);
  engine.NotifySduQueued();
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);
  EXPECT_TRUE(ContainersEqual(payload, *last_pdu));
  EXPECT_TRUE(engine.IsQueueEmpty());
}

TEST(BasicModeTxEngineTest, ProcessSduTransmitsMaximalSizedSdu) {
  ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  FakeTxChannel channel;
  channel.HandleSendFrame([&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  });

  constexpr size_t kMtu = 1;
  const StaticByteBuffer payload(1);
  channel.QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
  BasicModeTxEngine(kTestChannelId, kMtu, channel).NotifySduQueued();
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);
  EXPECT_TRUE(ContainersEqual(payload, *last_pdu));
}

TEST(BasicModeTxEngineTest, ProcessSduDropsOversizedSdu) {
  FakeTxChannel channel;
  size_t n_pdus = 0;
  channel.HandleSendFrame([&](auto) { ++n_pdus; });

  constexpr size_t kMtu = 1;
  channel.QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer(1, 2)));
  BasicModeTxEngine(kTestChannelId, kMtu, channel).NotifySduQueued();
  EXPECT_EQ(0u, n_pdus);
}

TEST(BasicModeTxEngineTest, ProcessSduSurvivesZeroByteSdu) {
  FakeTxChannel channel;
  size_t n_pdus = 0;
  channel.HandleSendFrame([&](auto) { ++n_pdus; });
  constexpr size_t kMtu = 1;
  channel.QueueSdu(std::make_unique<DynamicByteBuffer>());
  BasicModeTxEngine(kTestChannelId, kMtu, channel).NotifySduQueued();
  EXPECT_EQ(1u, n_pdus);
}

}  // namespace
}  // namespace bt::l2cap::internal
