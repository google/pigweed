// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"

namespace bt::hci {

EmbossCommandPacket EmbossCommandPacket::New(hci_spec::OpCode opcode, size_t packet_size) {
  BT_ASSERT_MSG(packet_size >= hci_spec::EmbossCommandHeader::IntrinsicSizeInBytes(),
                "command packet size must be at least 3 bytes to accomodate header");
  EmbossCommandPacket packet(packet_size);

  auto header = packet.view<hci_spec::EmbossCommandHeaderWriter>();
  header.opcode().BackingStorage().WriteUInt(opcode);
  header.parameter_total_size().Write(packet_size -
                                      hci_spec::EmbossCommandHeader::IntrinsicSizeInBytes());

  return packet;
}

hci_spec::OpCode EmbossCommandPacket::opcode() {
  auto header = view<hci_spec::EmbossCommandHeaderView>();
  BT_ASSERT(header.opcode().Ok());
  return header.opcode().BackingStorage().ReadUInt();
}

}  // namespace bt::hci
