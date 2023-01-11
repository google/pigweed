// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"

namespace bt::hci {

EmbossCommandPacket::EmbossCommandPacket(hci_spec::OpCode opcode, size_t packet_size)
    : DynamicPacket(packet_size) {
  BT_ASSERT_MSG(packet_size >= pw::bluetooth::emboss::CommandHeader::IntrinsicSizeInBytes(),
                "command packet size must be at least 3 bytes to accomodate header");
  auto header = view<pw::bluetooth::emboss::CommandHeaderWriter>();
  header.opcode().BackingStorage().WriteUInt(opcode);
  header.parameter_total_size().Write(packet_size -
                                      pw::bluetooth::emboss::CommandHeader::IntrinsicSizeInBytes());
}

hci_spec::OpCode EmbossCommandPacket::opcode() const {
  return header_view().opcode().BackingStorage().ReadUInt();
}

uint8_t EmbossCommandPacket::ogf() const { return header_view().opcode().ogf().Read(); }

uint16_t EmbossCommandPacket::ocf() const { return header_view().opcode().ocf().Read(); }

pw::bluetooth::emboss::CommandHeaderView EmbossCommandPacket::header_view() const {
  return view<pw::bluetooth::emboss::CommandHeaderView>();
}

EmbossEventPacket::EmbossEventPacket(size_t packet_size) : DynamicPacket(packet_size) {
  BT_ASSERT_MSG(packet_size >= pw::bluetooth::emboss::EventHeader::IntrinsicSizeInBytes(),
                "event packet size must be at least 2 bytes to accomodate header");
}

}  // namespace bt::hci
