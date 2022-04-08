// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "basic_mode_rx_engine.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/recombiner.h"

namespace bt::l2cap::internal {
namespace {

constexpr hci_spec::ConnectionHandle kTestHandle = 0x0001;
constexpr ChannelId kTestChannelId = 0x0001;

TEST(BasicModeRxEngineTest, ProcessPduReturnsSdu) {
  const StaticByteBuffer payload('h', 'e', 'l', 'l', 'o');
  const auto sdu = BasicModeRxEngine().ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kNoFcs));
  ASSERT_TRUE(sdu);
  EXPECT_TRUE(ContainersEqual(payload, *sdu));
}

TEST(BasicModeRxEngineTest, ProcessPduCanHandleZeroBytePayload) {
  const StaticByteBuffer byte_buf(0x01, 0x00, 0x04, 0x00,  // ACL data header
                                  0x00, 0x00, 0xFF, 0xFF   // Basic L2CAP header
  );
  auto hci_packet = hci::ACLDataPacket::New(byte_buf.size() - sizeof(hci_spec::ACLDataHeader));
  hci_packet->mutable_view()->mutable_data().Write(byte_buf);
  hci_packet->InitializeFromBuffer();

  Recombiner recombiner(kTestHandle);
  auto result = recombiner.ConsumeFragment(std::move(hci_packet));
  EXPECT_FALSE(result.frames_dropped);
  ASSERT_TRUE(result.pdu);

  ASSERT_TRUE(result.pdu->is_valid());
  ASSERT_EQ(1u, result.pdu->fragment_count());
  ASSERT_EQ(0u, result.pdu->length());

  const ByteBufferPtr sdu = BasicModeRxEngine().ProcessPdu(std::move(*result.pdu));
  ASSERT_TRUE(sdu);
  EXPECT_EQ(0u, sdu->size());
}

}  // namespace
}  // namespace bt::l2cap::internal
