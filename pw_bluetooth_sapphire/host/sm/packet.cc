// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packet.h"

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt {
namespace sm {

PacketReader::PacketReader(const ByteBuffer* buffer)
    : PacketView<Header>(buffer, buffer->size() - sizeof(Header)) {}

ValidPacketReader::ValidPacketReader(const ByteBuffer* buffer) : PacketReader(buffer) {}

fit::result<ValidPacketReader, sm::ErrorCode> ValidPacketReader::ParseSdu(
    const ByteBufferPtr& sdu) {
  ZX_ASSERT(sdu);
  size_t length = sdu->size();
  if (length < sizeof(Header)) {
    bt_log(DEBUG, "sm", "PDU too short!");
    return fit::error(ErrorCode::kInvalidParameters);
  }
  auto reader = PacketReader(sdu.get());
  auto expected_payload_size = kCodeToPayloadSize.find(reader.code());
  if (expected_payload_size == kCodeToPayloadSize.end()) {
    bt_log(DEBUG, "sm", "smp code not recognized: %#.2X", reader.code());
    return fit::error(ErrorCode::kCommandNotSupported);
  }
  if (reader.payload_size() != expected_payload_size->second) {
    bt_log(DEBUG, "sm", "malformed packet with code %#.2X", reader.code());
    return fit::error(ErrorCode::kInvalidParameters);
  }
  return fit::ok(ValidPacketReader(sdu.get()));
}

PacketWriter::PacketWriter(Code code, MutableByteBuffer* buffer)
    : MutablePacketView<Header>(buffer, buffer->size() - sizeof(Header)) {
  mutable_header()->code = code;
}

}  // namespace sm
}  // namespace bt
