// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SLAB_ALLOCATORS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SLAB_ALLOCATORS_H_

#include <memory>

#include <fbl/slab_allocator.h>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator_traits.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/packet.h"

// This file defines a fbl::SlabAllocator trait template that can be used to
// slab-allocate instances of hci_spec::Packet. It's signature is as follows:
//
//   template <typename HeaderType, size_t BufferSize, size_t NumBuffers>
//   using PacketTraits = ...;
//
// The following defines a SlabAllocator that returns instances of
// Packet<MyHeader>. Each packet is configured to be backed by a 256 octet
// buffer and each slab can allocate 128 packets:
//
// my_packet.h:
//   #include "packet.h"
//   #include "slab_allocators.h"
//   ...
//
//   using MyPacket = hci_spec::Packet<MyHeader>;
//   using MyPacketTraits =
//       hci::slab_allocators::PacketTraits<MyHeader, 256, 128>;
//   ...
//
// my_packet.cc:
//   DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(MyPacketTraits, my_max_num_slabs,
//   true);
//   ...
//
// foo.cc:
//   #include "my_packet.h"
//
//   std::unique_ptr<MyPacket> packet =
//       fbl::SlabAllocator<MyPacketTraits>::New(my_payload_size);
//
// If the header type provided to PacketTraits would correspond to an explicit
// specialization of the Packet template, then the specialization MUST provide a
// default constructor that is visible to all of its subclasses.
namespace bt::hci::slab_allocators {

// Slab sizes for control (command/event) and ACL data packets used by the slab
// allocators. These are used by the CommandPacket, EventPacket, and
// ACLDataPacket classes.

// TODO(armansito): The slab sizes below are arbitrary; fine tune them based on
// usage.
constexpr size_t kMaxControlSlabSize = 65536;  // 64K
constexpr size_t kMaxACLSlabSize = 65536;      // 64K
constexpr size_t kMaxScoSlabSize = 33024;      // exactly 128 max size SCO packets
constexpr size_t kMaxNumSlabs = 100;

// The largest possible control packet size.
constexpr size_t kLargeControlPayloadSize = hci_spec::kMaxCommandPacketPayloadSize;
constexpr size_t kLargeControlPacketSize =
    sizeof(hci_spec::CommandHeader) + kLargeControlPayloadSize;
constexpr size_t kNumLargeControlPackets = kMaxControlSlabSize / kLargeControlPacketSize;

// The average HCI control packet payload size. Most packets are under 16 bytes.
constexpr size_t kSmallControlPayloadSize = 16;
constexpr size_t kSmallControlPacketSize =
    sizeof(hci_spec::CommandHeader) + kSmallControlPayloadSize;
constexpr size_t kNumSmallControlPackets = kMaxControlSlabSize / kSmallControlPacketSize;

// Large, medium, and small buffer sizes for ACL data packets.
constexpr size_t kLargeACLDataPayloadSize = hci_spec::kMaxACLPayloadSize;
constexpr size_t kLargeACLDataPacketSize =
    sizeof(hci_spec::ACLDataHeader) + kLargeACLDataPayloadSize;
constexpr size_t kNumLargeACLDataPackets = kMaxACLSlabSize / kLargeACLDataPacketSize;

constexpr size_t kMediumACLDataPayloadSize = 256;
constexpr size_t kMediumACLDataPacketSize =
    sizeof(hci_spec::ACLDataHeader) + kMediumACLDataPayloadSize;
constexpr size_t kNumMediumACLDataPackets = kMaxACLSlabSize / kMediumACLDataPacketSize;

constexpr size_t kSmallACLDataPayloadSize = 64;
constexpr size_t kSmallACLDataPacketSize =
    sizeof(hci_spec::ACLDataHeader) + kSmallACLDataPayloadSize;
constexpr size_t kNumSmallACLDataPackets = kMaxACLSlabSize / kSmallACLDataPacketSize;

constexpr size_t kMaxScoDataPayloadSize = hci_spec::kMaxSynchronousDataPacketPayloadSize;
constexpr size_t kMaxScoDataPacketSize =
    sizeof(hci_spec::SynchronousDataHeader) + kMaxScoDataPayloadSize;
constexpr size_t kNumMaxScoDataPackets = kMaxScoSlabSize / kMaxScoDataPacketSize;

namespace internal {

template <size_t BufferSize>
class FixedSizePacketStorage {
 protected:
  StaticByteBuffer<BufferSize> buffer_;
};

// A FixedSizePacket provides fixed-size buffer storage for Packets and is the basis for a
// slab-allocated Packet. Multiple inheritance is required to initialize the underlying buffer
// before PacketBase.
template <typename HeaderType, size_t BufferSize>
class FixedSizePacket : public FixedSizePacketStorage<BufferSize>, public Packet<HeaderType> {
 public:
  explicit FixedSizePacket(size_t payload_size = 0u)
      : Packet<HeaderType>(MutablePacketView<HeaderType>(&this->buffer_, payload_size)) {}

  ~FixedSizePacket() override = default;

  FixedSizePacket(const FixedSizePacket&) = delete;
  FixedSizePacket& operator=(const FixedSizePacket&) = delete;
};

template <typename HeaderType, size_t BufferSize, size_t NumBuffers>
class SlabPacket;

}  // namespace internal

template <typename HeaderType, size_t BufferSize, size_t NumBuffers>
using PacketTraits =
    SlabAllocatorTraits<internal::SlabPacket<HeaderType, BufferSize, NumBuffers>,
                        sizeof(internal::FixedSizePacket<HeaderType, BufferSize>), NumBuffers>;

namespace internal {

template <typename HeaderType, size_t BufferSize, size_t NumBuffers>
class SlabPacket : public FixedSizePacket<HeaderType, BufferSize>,
                   public fbl::SlabAllocated<PacketTraits<HeaderType, BufferSize, NumBuffers>> {
 public:
  explicit SlabPacket(size_t payload_size = 0u)
      : FixedSizePacket<HeaderType, BufferSize>(payload_size) {}

 private:
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SlabPacket);
};

}  // namespace internal

}  // namespace bt::hci::slab_allocators

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SLAB_ALLOCATORS_H_
