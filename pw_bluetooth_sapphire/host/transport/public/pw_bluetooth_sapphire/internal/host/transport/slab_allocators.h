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
#include <pw_bluetooth/hci_common.emb.h>
#include <pw_bluetooth/hci_data.emb.h>

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/packet.h"

namespace bt::hci::allocators {

// Slab sizes for control (command/event) and ACL data packets used by the slab
// allocators. These are used by the EventPacket, and ACLDataPacket classes.

// TODO(armansito): The slab sizes below are arbitrary; fine tune them based on
// usage.
inline constexpr size_t kMaxControlSlabSize = 65536;  // 64K
inline constexpr size_t kMaxACLSlabSize = 65536;      // 64K
inline constexpr size_t kMaxScoSlabSize =
    33024;  // exactly 128 max size SCO packets
inline constexpr size_t kMaxNumSlabs = 100;

// The largest possible control packet size.
inline constexpr size_t kMaxEventPayloadSize =
    hci_spec::kMaxEventPacketPayloadSize;
inline constexpr size_t kMaxEventPacketSize =
    pw::bluetooth::emboss::EventHeader::IntrinsicSizeInBytes() +
    kMaxEventPayloadSize;
inline constexpr size_t kMaxNumEventPackets =
    kMaxControlSlabSize / kMaxEventPacketSize;

// Large, medium, and small buffer sizes for ACL data packets.
inline constexpr size_t kLargeACLDataPayloadSize = hci_spec::kMaxACLPayloadSize;
inline constexpr size_t kLargeACLDataPacketSize =
    pw::bluetooth::emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
    kLargeACLDataPayloadSize;
inline constexpr size_t kNumLargeACLDataPackets =
    kMaxACLSlabSize / kLargeACLDataPacketSize;

inline constexpr size_t kMediumACLDataPayloadSize = 256;
inline constexpr size_t kMediumACLDataPacketSize =
    pw::bluetooth::emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
    kMediumACLDataPayloadSize;
inline constexpr size_t kNumMediumACLDataPackets =
    kMaxACLSlabSize / kMediumACLDataPacketSize;

inline constexpr size_t kSmallACLDataPayloadSize = 64;
inline constexpr size_t kSmallACLDataPacketSize =
    pw::bluetooth::emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
    kSmallACLDataPayloadSize;
inline constexpr size_t kNumSmallACLDataPackets =
    kMaxACLSlabSize / kSmallACLDataPacketSize;

inline constexpr size_t kMaxScoDataPayloadSize =
    hci_spec::kMaxSynchronousDataPacketPayloadSize;
inline constexpr size_t kMaxScoDataPacketSize =
    sizeof(hci_spec::SynchronousDataHeader) + kMaxScoDataPayloadSize;
inline constexpr size_t kNumMaxScoDataPackets =
    kMaxScoSlabSize / kMaxScoDataPacketSize;

namespace internal {

template <size_t BufferSize>
class FixedSizePacketStorage {
 protected:
  StaticByteBuffer<BufferSize> buffer_;
};

// A FixedSizePacket provides fixed-size buffer storage for Packets and is the
// basis for a slab-allocated Packet. Multiple inheritance is required to
// initialize the underlying buffer before PacketBase.
template <typename HeaderType, size_t BufferSize>
class FixedSizePacket : public FixedSizePacketStorage<BufferSize>,
                        public Packet<HeaderType> {
 public:
  explicit FixedSizePacket(size_t payload_size = 0u)
      : Packet<HeaderType>(
            MutablePacketView<HeaderType>(&this->buffer_, payload_size)) {}

  ~FixedSizePacket() override = default;

  FixedSizePacket(const FixedSizePacket&) = delete;
  FixedSizePacket& operator=(const FixedSizePacket&) = delete;
};

}  // namespace internal

}  // namespace bt::hci::allocators
