// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SLAB_ALLOCATORS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SLAB_ALLOCATORS_H_

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/packet.h"

namespace bt::hci::allocators {

// Slab sizes for control (command/event) and ACL data packets used by the slab
// allocators. These are used by the CommandPacket, EventPacket, and
// ACLDataPacket classes.

// TODO(armansito): The slab sizes below are arbitrary; fine tune them based on
// usage.
constexpr size_t kMaxControlSlabSize = 65536;  // 64K
constexpr size_t kMaxACLSlabSize = 65536;      // 64K
constexpr size_t kMaxScoSlabSize = 33024;  // exactly 128 max size SCO packets
constexpr size_t kMaxNumSlabs = 100;

// The largest possible control packet size.
constexpr size_t kLargeControlPayloadSize =
    hci_spec::kMaxCommandPacketPayloadSize;
constexpr size_t kLargeControlPacketSize =
    sizeof(hci_spec::CommandHeader) + kLargeControlPayloadSize;
constexpr size_t kNumLargeControlPackets =
    kMaxControlSlabSize / kLargeControlPacketSize;

// The average HCI control packet payload size. Most packets are under 16 bytes.
constexpr size_t kSmallControlPayloadSize = 16;
constexpr size_t kSmallControlPacketSize =
    sizeof(hci_spec::CommandHeader) + kSmallControlPayloadSize;
constexpr size_t kNumSmallControlPackets =
    kMaxControlSlabSize / kSmallControlPacketSize;

// Large, medium, and small buffer sizes for ACL data packets.
constexpr size_t kLargeACLDataPayloadSize = hci_spec::kMaxACLPayloadSize;
constexpr size_t kLargeACLDataPacketSize =
    sizeof(hci_spec::ACLDataHeader) + kLargeACLDataPayloadSize;
constexpr size_t kNumLargeACLDataPackets =
    kMaxACLSlabSize / kLargeACLDataPacketSize;

constexpr size_t kMediumACLDataPayloadSize = 256;
constexpr size_t kMediumACLDataPacketSize =
    sizeof(hci_spec::ACLDataHeader) + kMediumACLDataPayloadSize;
constexpr size_t kNumMediumACLDataPackets =
    kMaxACLSlabSize / kMediumACLDataPacketSize;

constexpr size_t kSmallACLDataPayloadSize = 64;
constexpr size_t kSmallACLDataPacketSize =
    sizeof(hci_spec::ACLDataHeader) + kSmallACLDataPayloadSize;
constexpr size_t kNumSmallACLDataPackets =
    kMaxACLSlabSize / kSmallACLDataPacketSize;

constexpr size_t kMaxScoDataPayloadSize =
    hci_spec::kMaxSynchronousDataPacketPayloadSize;
constexpr size_t kMaxScoDataPacketSize =
    sizeof(hci_spec::SynchronousDataHeader) + kMaxScoDataPayloadSize;
constexpr size_t kNumMaxScoDataPackets =
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

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SLAB_ALLOCATORS_H_
