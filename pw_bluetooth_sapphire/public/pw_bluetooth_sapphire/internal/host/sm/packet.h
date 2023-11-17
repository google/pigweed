// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PACKET_H_

#include <lib/fit/result.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"

namespace bt::sm {

// Utilities for processing SMP packets.
// TODO(fxbug.dev/48978): Merge PacketReader & ValidPacketReader types into one
// type for validating & accessing SM packets once PacketReader is no longer
// used.
class PacketReader : public PacketView<Header> {
 public:
  explicit PacketReader(const ByteBuffer* buffer);
  inline Code code() const { return header().code; }
};

// A type which has been verified to satisfy all the preconditions of a valid
// SMP packet. Namely, 1.) The packet's length is at least that of an SMP
// header. 2.) The packet's header code is a valid SMP code that our stack
// supports. 3.) The length of the packet's payload matches the payload
// associated with its header code.
class ValidPacketReader : public PacketReader {
 public:
  // Convert a ByteBufferPtr to a ValidPacketReader if possible to allow
  // unchecked access to its payload, or an error explaining why we could not.
  static fit::result<ErrorCode, ValidPacketReader> ParseSdu(
      const ByteBufferPtr& sdu);

 private:
  // Private constructor because a valid PacketReader must be parsed from a
  // ByteBufferPtr
  explicit ValidPacketReader(const ByteBuffer* buffer);
};

class PacketWriter : public MutablePacketView<Header> {
 public:
  // Constructor writes |code| into |buffer|.
  PacketWriter(Code code, MutableByteBuffer* buffer);
};

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PACKET_H_
