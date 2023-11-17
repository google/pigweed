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

#include "pw_bluetooth_sapphire/internal/host/transport/sco_data_packet.h"

#include <endian.h>

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/transport/slab_allocators.h"

namespace bt::hci {
// Type containing both a fixed packet storage buffer and a ScoDataPacket
// interface to the buffer.
using MaxScoDataPacket =
    allocators::internal::FixedSizePacket<hci_spec::SynchronousDataHeader,
                                          allocators::kMaxScoDataPacketSize>;

std::unique_ptr<ScoDataPacket> ScoDataPacket::New(uint8_t payload_size) {
  return std::make_unique<MaxScoDataPacket>(payload_size);
}

std::unique_ptr<ScoDataPacket> ScoDataPacket::New(
    hci_spec::ConnectionHandle connection_handle, uint8_t payload_size) {
  std::unique_ptr<ScoDataPacket> packet = ScoDataPacket::New(payload_size);
  packet->WriteHeader(connection_handle);
  return packet;
}

hci_spec::ConnectionHandle ScoDataPacket::connection_handle() const {
  // Return the lower 12-bits of the first two octets.
  return le16toh(view().header().handle_and_flags) & 0x0FFF;
}
hci_spec::SynchronousDataPacketStatusFlag ScoDataPacket::packet_status_flag()
    const {
  // Return bits 4-5 in the higher octet of |handle_and_flags|, i.e.
  // 0b00xx000000000000.
  return static_cast<hci_spec::SynchronousDataPacketStatusFlag>(
      (le16toh(view().header().handle_and_flags) >> 12) & 0x0003);
}

void ScoDataPacket::InitializeFromBuffer() {
  mutable_view()->Resize(
      /*payload_size=*/le16toh(view().header().data_total_length));
}

void ScoDataPacket::WriteHeader(hci_spec::ConnectionHandle connection_handle) {
  // Handle must fit inside 12-bits.
  BT_ASSERT(connection_handle <= 0x0FFF);
  // This sets the Packet Status Flag (upper bits of handle_and_flags) to 0,
  // which is required for Host->Controller SCO packets.
  mutable_view()->mutable_header()->handle_and_flags =
      htole16(connection_handle);
  mutable_view()->mutable_header()->data_total_length =
      static_cast<uint8_t>(view().payload_size());
}

}  // namespace bt::hci
