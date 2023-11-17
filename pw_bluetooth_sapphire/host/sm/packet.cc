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

#include "pw_bluetooth_sapphire/internal/host/sm/packet.h"

#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::sm {

PacketReader::PacketReader(const ByteBuffer* buffer)
    : PacketView<Header>(buffer, buffer->size() - sizeof(Header)) {}

ValidPacketReader::ValidPacketReader(const ByteBuffer* buffer)
    : PacketReader(buffer) {}

fit::result<ErrorCode, ValidPacketReader> ValidPacketReader::ParseSdu(
    const ByteBufferPtr& sdu) {
  BT_ASSERT(sdu);
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

}  // namespace bt::sm
