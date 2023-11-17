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

#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_packet.h"

#include <endian.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/transport/slab_allocators.h"

namespace bt::hci {
namespace {

// Type containing both a fixed packet storage buffer and a ACLDataPacket
// interface to the buffer. Limit to 3 template instantiations: small, medium,
// and large.
using SmallACLDataPacket =
    allocators::internal::FixedSizePacket<hci_spec::ACLDataHeader,
                                          allocators::kSmallACLDataPacketSize>;
using MediumACLDataPacket =
    allocators::internal::FixedSizePacket<hci_spec::ACLDataHeader,
                                          allocators::kMediumACLDataPacketSize>;
using LargeACLDataPacket =
    allocators::internal::FixedSizePacket<hci_spec::ACLDataHeader,
                                          allocators::kLargeACLDataPacketSize>;

ACLDataPacketPtr NewACLDataPacket(size_t payload_size) {
  BT_ASSERT_MSG(payload_size <= allocators::kLargeACLDataPayloadSize,
                "payload size %zu too large (allowed = %zu)",
                payload_size,
                allocators::kLargeACLDataPayloadSize);

  if (payload_size <= allocators::kSmallACLDataPayloadSize) {
    return std::make_unique<SmallACLDataPacket>(payload_size);
  }

  if (payload_size <= allocators::kMediumACLDataPayloadSize) {
    return std::make_unique<MediumACLDataPacket>(payload_size);
  }

  return std::make_unique<LargeACLDataPacket>(payload_size);
}

}  // namespace

// static
ACLDataPacketPtr ACLDataPacket::New(uint16_t payload_size) {
  return NewACLDataPacket(payload_size);
}

// static
ACLDataPacketPtr ACLDataPacket::New(
    hci_spec::ConnectionHandle connection_handle,
    hci_spec::ACLPacketBoundaryFlag packet_boundary_flag,
    hci_spec::ACLBroadcastFlag broadcast_flag,
    uint16_t payload_size) {
  auto packet = NewACLDataPacket(payload_size);
  if (!packet)
    return nullptr;

  packet->WriteHeader(connection_handle, packet_boundary_flag, broadcast_flag);
  return packet;
}

hci_spec::ConnectionHandle ACLDataPacket::connection_handle() const {
  // Return the lower 12-bits of the first two octets.
  return le16toh(ACLDataPacket::view().header().handle_and_flags) & 0x0FFF;
}

hci_spec::ACLPacketBoundaryFlag ACLDataPacket::packet_boundary_flag() const {
  // Return bits 4-5 in the higher octet of |handle_and_flags| or
  // "0b00xx000000000000".
  return static_cast<hci_spec::ACLPacketBoundaryFlag>(
      (le16toh(ACLDataPacket::view().header().handle_and_flags) >> 12) &
      0x0003);
}

hci_spec::ACLBroadcastFlag ACLDataPacket::broadcast_flag() const {
  // Return bits 6-7 in the higher octet of |handle_and_flags| or
  // "0bxx00000000000000".
  return static_cast<hci_spec::ACLBroadcastFlag>(
      le16toh(view().header().handle_and_flags) >> 14);
}

void ACLDataPacket::InitializeFromBuffer() {
  mutable_view()->Resize(le16toh(view().header().data_total_length));
}

void ACLDataPacket::WriteHeader(
    hci_spec::ConnectionHandle connection_handle,
    hci_spec::ACLPacketBoundaryFlag packet_boundary_flag,
    hci_spec::ACLBroadcastFlag broadcast_flag) {
  // Must fit inside 12-bits.
  BT_DEBUG_ASSERT(connection_handle <= 0x0FFF);

  // Must fit inside 2-bits.
  BT_DEBUG_ASSERT(static_cast<uint8_t>(packet_boundary_flag) <= 0x03);
  BT_DEBUG_ASSERT(static_cast<uint8_t>(broadcast_flag) <= 0x03);

  // Bitwise OR causes int promotion, so the result must be explicitly casted.
  uint16_t handle_and_flags = static_cast<uint16_t>(
      connection_handle | (static_cast<uint16_t>(packet_boundary_flag) << 12) |
      (static_cast<uint16_t>(broadcast_flag) << 14));
  mutable_view()->mutable_header()->handle_and_flags =
      htole16(handle_and_flags);
  mutable_view()->mutable_header()->data_total_length =
      htole16(view().payload_size());
}

}  // namespace bt::hci
