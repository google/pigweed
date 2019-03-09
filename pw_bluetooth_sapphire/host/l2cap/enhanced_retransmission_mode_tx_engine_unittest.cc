// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enhanced_retransmission_mode_tx_engine.h"

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "gtest/gtest.h"

namespace btlib {
namespace l2cap {
namespace internal {
namespace {

constexpr ChannelId kTestChannelId = 0x0001;

using TxEngine = EnhancedRetransmissionModeTxEngine;

TEST(L2CAP_EnhancedRetransmissionModeTxEngineTest,
     QueueSduTransmitsMinimalSizedSdu) {
  common::ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };

  constexpr size_t kMtu = 10;
  const auto payload = common::CreateStaticByteBuffer(1);
  TxEngine(kTestChannelId, kMtu, tx_callback)
      .QueueSdu(std::make_unique<common::DynamicByteBuffer>(payload));
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);
  EXPECT_TRUE(common::ContainersEqual(payload, *last_pdu));
}

TEST(L2CAP_EnhancedRetransmissionModeTxEngineTest,
     QueueSduTransmitsMaximalSizedSdu) {
  common::ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };

  constexpr size_t kMtu = 1;
  const auto payload = common::CreateStaticByteBuffer(1);
  TxEngine(kTestChannelId, kMtu, tx_callback)
      .QueueSdu(std::make_unique<common::DynamicByteBuffer>(payload));
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);
  EXPECT_TRUE(common::ContainersEqual(payload, *last_pdu));
}

TEST(L2CAP_EnhancedRetransmissionModeTxEngineTest,
     QueueSduSurvivesOversizedSdu) {
  // TODO(BT-440): Update this test when we add support for segmentation.
  constexpr size_t kMtu = 1;
  TxEngine(kTestChannelId, kMtu, [](auto pdu) {})
      .QueueSdu(std::make_unique<common::DynamicByteBuffer>(
          common::CreateStaticByteBuffer(1, 2)));
}

TEST(L2CAP_EnhancedRetransmissionModeTxEngineTest,
     QueueSduSurvivesZeroByteSdu) {
  constexpr size_t kMtu = 1;
  TxEngine(kTestChannelId, kMtu, [](auto pdu) {
  }).QueueSdu(std::make_unique<common::DynamicByteBuffer>());
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace btlib
