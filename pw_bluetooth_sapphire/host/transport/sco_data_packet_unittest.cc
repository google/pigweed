// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
