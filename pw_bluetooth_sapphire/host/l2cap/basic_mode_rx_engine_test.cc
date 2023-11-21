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

#include "pw_bluetooth_sapphire/internal/host/l2cap/basic_mode_rx_engine.h"

#include <gtest/gtest.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/fragmenter.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/recombiner.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::l2cap::internal {
namespace {

constexpr hci_spec::ConnectionHandle kTestHandle = 0x0001;
constexpr ChannelId kTestChannelId = 0x0001;

TEST(BasicModeRxEngineTest, ProcessPduReturnsSdu) {
  const StaticByteBuffer payload('h', 'e', 'l', 'l', 'o');
  const auto sdu = BasicModeRxEngine().ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(
              kTestChannelId, payload, FrameCheckSequenceOption::kNoFcs));
  ASSERT_TRUE(sdu);
  EXPECT_TRUE(ContainersEqual(payload, *sdu));
}

TEST(BasicModeRxEngineTest, ProcessPduCanHandleZeroBytePayload) {
  const StaticByteBuffer byte_buf(0x01,
                                  0x00,
                                  0x04,
                                  0x00,  // ACL data header
                                  0x00,
                                  0x00,
                                  0xFF,
                                  0xFF  // Basic L2CAP header
  );
  auto hci_packet = hci::ACLDataPacket::New(byte_buf.size() -
                                            sizeof(hci_spec::ACLDataHeader));
  hci_packet->mutable_view()->mutable_data().Write(byte_buf);
  hci_packet->InitializeFromBuffer();

  Recombiner recombiner(kTestHandle);
  auto result = recombiner.ConsumeFragment(std::move(hci_packet));
  EXPECT_FALSE(result.frames_dropped);
  ASSERT_TRUE(result.pdu);

  ASSERT_TRUE(result.pdu->is_valid());
  ASSERT_EQ(1u, result.pdu->fragment_count());
  ASSERT_EQ(0u, result.pdu->length());

  const ByteBufferPtr sdu =
      BasicModeRxEngine().ProcessPdu(std::move(*result.pdu));
  ASSERT_TRUE(sdu);
  EXPECT_EQ(0u, sdu->size());
}

}  // namespace
}  // namespace bt::l2cap::internal
