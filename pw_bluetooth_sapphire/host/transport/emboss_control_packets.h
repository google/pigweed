// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_CONTROL_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_CONTROL_PACKETS_H_

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_packet.h"

#include <src/connectivity/bluetooth/core/bt-host/hci-spec/hci-protocol.emb.h>

namespace bt::hci {

template <class ViewT>
class EmbossCommandPacketT;

// EmbossCommandPacket is the HCI Command packet specialization of DynamicPacket.
class EmbossCommandPacket : public DynamicPacket {
 public:
  // Construct an HCI Command packet from an Emboss view T and initialize its header with the
  // |opcode| and size.
  template <typename T>
  static EmbossCommandPacketT<T> New(hci_spec::OpCode opcode) {
    return New<T>(opcode, T::IntrinsicSizeInBytes().Read());
  }

  // Construct an HCI Command packet of |packet_size| total bytes (header + payload) and initialize
  // its header with the |opcode| and size. This constructor is meant for variable size packets, for
  // which clients must calculate packet size manually.
  template <typename T>
  static EmbossCommandPacketT<T> New(hci_spec::OpCode opcode, size_t packet_size) {
    EmbossCommandPacketT<T> packet(opcode, packet_size);
    return packet;
  }

  hci_spec::OpCode opcode() const;
  // Returns the OGF (OpCode Group Field) which occupies the upper 6-bits of the opcode.
  uint8_t ogf() const;
  // Returns the OCF (OpCode Command Field) which occupies the lower 10-bits of the opcode.
  uint16_t ocf() const;

 protected:
  explicit EmbossCommandPacket(hci_spec::OpCode opcode, size_t packet_size);

 private:
  hci_spec::EmbossCommandHeaderView header_view() const;
};

// Helper subclass that remembers the view type it was constructed with. It is safe to slice
// an EmbossCommandPacketT into an EmbossCommandPacket.
template <class ViewT>
class EmbossCommandPacketT : public EmbossCommandPacket {
 public:
  ViewT view_t() { return view<ViewT>(); }

 private:
  friend class EmbossCommandPacket;

  EmbossCommandPacketT(hci_spec::OpCode opcode, size_t packet_size)
      : EmbossCommandPacket(opcode, packet_size) {}
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_EMBOSS_CONTROL_PACKETS_H_
