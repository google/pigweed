// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_CONTROL_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_CONTROL_PACKETS_H_

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_packet.h"

#include <src/connectivity/bluetooth/core/bt-host/hci-spec/hci-protocol.emb.h>

namespace bt::hci {

// EmbossCommandPacket is the HCI Command packet specialization of EmbossPacket.
class EmbossCommandPacket : public EmbossPacket {
 public:
  // Construct an HCI Command packet from an Emboss view T and initialize its header with the
  // |opcode| and size.
  template <typename T>
  static EmbossCommandPacket New(hci_spec::OpCode opcode) {
    return New(opcode, T::IntrinsicSizeInBytes().Read());
  }

  // Construct an HCI Command packet of |packet_size| total bytes (header + payload) and initialize
  // its header with the |opcode| and size. This constructor is meant for variable size packets, for
  // which clients must calculate packet size manually.
  static EmbossCommandPacket New(hci_spec::OpCode opcode, size_t packet_size);

  hci_spec::OpCode opcode();
  // Returns the OGF (OpCode Group Field) which occupies the upper 6-bits of the opcode.
  uint8_t ogf() { return opcode() >> 10; }
  // Returns the OCF (OpCode Command Field) which occupies the lower 10-bits of the opcode.
  uint16_t ocf() { return opcode() & 0x3FF; }

 private:
  explicit EmbossCommandPacket(size_t packet_size) : EmbossPacket(packet_size) {}
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_CONTROL_PACKETS_H_
