// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/att/packet.h"

namespace bt::att {

PacketReader::PacketReader(const ByteBuffer* buffer)
    : PacketView<Header>(buffer, buffer->size() - sizeof(Header)) {}

PacketWriter::PacketWriter(OpCode opcode, MutableByteBuffer* buffer)
    : MutablePacketView<Header>(buffer, buffer->size() - sizeof(Header)) {
  mutable_header()->opcode = opcode;
}

}  // namespace bt::att
