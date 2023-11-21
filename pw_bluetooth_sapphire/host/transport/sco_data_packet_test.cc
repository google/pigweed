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

#include "pw_bluetooth_sapphire/internal/host/transport/sco_data_packet.h"

#include <gtest/gtest.h>

namespace bt::hci {
namespace {

TEST(ScoPacketTest, NewWithConnectionHandle) {
  const hci_spec::ConnectionHandle handle = 0x000F;
  const std::unique_ptr<const ScoDataPacket> packet =
      ScoDataPacket::New(/*connection_handle=*/handle, /*payload_size=*/1);
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->connection_handle(), handle);
  EXPECT_EQ(packet->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kCorrectlyReceived);
}

TEST(ScoPacketTest, ReadFromBufferWithStatusFlag) {
  StaticByteBuffer bytes(0x02,  // handle
                         0x00,  // status flag: correctly received data
                         0x01,  // data total length
                         0x09   // payload
  );
  std::unique_ptr<ScoDataPacket> packet =
      ScoDataPacket::New(/*payload_size=*/1);
  ASSERT_TRUE(packet);
  packet->mutable_view()->mutable_data().Write(bytes);
  packet->InitializeFromBuffer();
  EXPECT_EQ(packet->connection_handle(), 0x0002);
  EXPECT_EQ(packet->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kCorrectlyReceived);
  EXPECT_EQ(packet->view().payload_size(), 1u);

  // Set packet status byte to kPossiblyInvalid
  packet->mutable_view()->mutable_data()[1] = 0b0001'0000;
  EXPECT_EQ(packet->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kPossiblyInvalid);

  // Set packet status byte to kNoDataReceived
  packet->mutable_view()->mutable_data()[1] = 0b0010'0000;
  EXPECT_EQ(packet->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kNoDataReceived);

  // Set packet status byte to kDataPartiallyLost
  packet->mutable_view()->mutable_data()[1] = 0b0011'0000;
  EXPECT_EQ(packet->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kDataPartiallyLost);
}

}  // namespace
}  // namespace bt::hci
