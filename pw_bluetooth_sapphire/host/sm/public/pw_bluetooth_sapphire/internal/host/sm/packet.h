// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once
#include <lib/fit/result.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"

namespace bt::sm {

// Utilities for processing SMP packets.
// TODO(fxbug.dev/42125894): Merge PacketReader & ValidPacketReader types into
// one type for validating & accessing SM packets once PacketReader is no longer
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
