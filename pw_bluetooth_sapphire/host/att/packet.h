// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PACKET_H_

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"

namespace btlib {
namespace att {

// Utilities for processing ATT protocol Packets.

class PacketReader : public common::PacketView<Header> {
 public:
  explicit PacketReader(const common::ByteBuffer* buffer);

  inline OpCode opcode() const { return header().opcode; }
};

class PacketWriter : public common::MutablePacketView<Header> {
 public:
  // Constructor writes |opcode| into |buffer|.
  PacketWriter(OpCode opcode, common::MutableByteBuffer* buffer);
};

}  // namespace att
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PACKET_H_
