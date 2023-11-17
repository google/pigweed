// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PACKET_H_

#include "pw_bluetooth_sapphire/internal/host/att/att.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"

namespace bt::att {

// Utilities for processing ATT protocol Packets.

class PacketReader : public PacketView<Header> {
 public:
  explicit PacketReader(const ByteBuffer* buffer);

  inline OpCode opcode() const { return header().opcode; }
};

class PacketWriter : public MutablePacketView<Header> {
 public:
  // Constructor writes |opcode| into |buffer|.
  PacketWriter(OpCode opcode, MutableByteBuffer* buffer);
};

}  // namespace bt::att

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_PACKET_H_
