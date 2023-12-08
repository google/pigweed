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
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_unit_test/framework.h"

namespace bt::l2cap::internal {
namespace {

constexpr ChannelId kTestChannelId = 0x0001;

TEST(BasicModeTxEngineTest, QueueSduTransmitsMinimalSizedSdu) {
  ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };

  constexpr size_t kMtu = 10;
  const StaticByteBuffer payload(1);
  BasicModeTxEngine(kTestChannelId, kMtu, tx_callback)
      .QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);
  EXPECT_TRUE(ContainersEqual(payload, *last_pdu));
}

TEST(BasicModeTxEngineTest, QueueSduTransmitsMaximalSizedSdu) {
  ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };

  constexpr size_t kMtu = 1;
  const StaticByteBuffer payload(1);
  BasicModeTxEngine(kTestChannelId, kMtu, tx_callback)
      .QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);
  EXPECT_TRUE(ContainersEqual(payload, *last_pdu));
}

TEST(BasicModeTxEngineTest, QueueSduDropsOversizedSdu) {
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };

  constexpr size_t kMtu = 1;
  BasicModeTxEngine(kTestChannelId, kMtu, tx_callback)
      .QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer(1, 2)));
  EXPECT_EQ(0u, n_pdus);
}

TEST(BasicModeTxEngineTest, QueueSduSurvivesZeroByteSdu) {
  constexpr size_t kMtu = 1;
  BasicModeTxEngine(kTestChannelId, kMtu, [](auto pdu) {
  }).QueueSdu(std::make_unique<DynamicByteBuffer>());
}

}  // namespace
}  // namespace bt::l2cap::internal
