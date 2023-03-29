// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "emboss_control_packets.h"

#include <gtest/gtest.h>

namespace bt::hci {

TEST(EmbossControlPackets, TooSmallCommandCompleteEventWhenReadingStatus) {
  StaticByteBuffer buffer(hci_spec::kCommandCompleteEventCode,
                          0x03,       // size
                          0x01,       // num hci packets
                          0x02, 0x03  // command opcode
                                      // There should be a status field here, but there isn't
  );
  EmbossEventPacket packet = EmbossEventPacket::New(buffer.size());
  packet.mutable_data().Write(buffer);
  EXPECT_FALSE(packet.StatusCode().has_value());
}

TEST(EmbossControlPackets, CommandCompleteEventWithStatus) {
  StaticByteBuffer buffer(hci_spec::kCommandCompleteEventCode,
                          0x04,        // size
                          0x01,        // num hci packets
                          0x02, 0x03,  // command opcode
                          0x00         // status (success)
  );
  EmbossEventPacket packet = EmbossEventPacket::New(buffer.size());
  packet.mutable_data().Write(buffer);
  ASSERT_TRUE(packet.StatusCode().has_value());
  EXPECT_EQ(packet.StatusCode().value(), pw::bluetooth::emboss::StatusCode::SUCCESS);
}

}  // namespace bt::hci
