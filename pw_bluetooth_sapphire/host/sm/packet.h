// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PACKET_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"

namespace bt {
namespace sm {

// Utilities for processing SMP packets.

class PacketReader : public PacketView<Header> {
 public:
  explicit PacketReader(const ByteBuffer* buffer);
  inline Code code() const { return header().code; }
};

class PacketWriter : public MutablePacketView<Header> {
 public:
  // Constructor writes |code| into |buffer|.
  PacketWriter(Code code, MutableByteBuffer* buffer);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PACKET_H_
