// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_PACKET_H_

#include <memory>

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/linked_list.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "lib/fxl/macros.h"

namespace btlib {
namespace hci {

// A Packet is a move-only object that can be used to hold sent and received HCI
// packets. The Packet template is parameterized over the protocol packet header
// type.
//
// Instances of Packet cannot be created directly as the template does not
// specify the backing buffer, which should be provided by a subclass.
//
// Header-type-specific functionality can be provided in specializations of the
// Packet template.
//
// USAGE:
//
//   Each Packet consists of a PacketView into a buffer that actually stores the
//   data. A buffer should be provided in a subclass implementation. While the
//   buffer must be sufficiently large to store the packet, the packet contents
//   can be much smaller.
//
//     template <typename HeaderType, size_t BufferSize>
//     class FixedBufferPacket : public Packet<HeaderType> {
//      public:
//       void Init(size_t payload_size) {
//         this->init_view(common::MutablePacketView<HeaderType>(&buffer_,
//         payload_size));
//       }
//
//      private:
//       common::StaticByteBuffer<BufferSize> buffer_;
//     };
//
//     std::unique_ptr<Packet<MyHeaderType>> packet =
//         std::make_unique<FixedBufferPacket<MyHeaderType, 255>>(payload_size);
//
//   Use Packet::view() to obtain a read-only view into the packet contents:
//
//     auto foo = packet->view().header().some_header_field;
//
//   Use Packet::mutable_view() to obtain a mutable view into the packet, which
//   allows the packet contents and the size of the packet to be modified:
//
//     packet->mutable_view()->mutable_header()->some_header_field = foo;
//     packet->mutable_view()->set_payload_size(my_new_size);
//
//     // Copy data directly into the buffer.
//     auto mutable_bytes = packet->mutable_view()->mutable_bytes();
//     std::memcpy(mutable_bytes.mutable_data(), data, mutable_bytes.size());
//
// SPECIALIZATIONS:
//
//   Additional functionality that is specific to a protocol header type can be
//   provided in a specialization of the Packet template.
//
//     using MagicPacket = Packet<MagicHeader>;
//
//     template <>
//     class Packet<MagicHeader> : public PacketBase<MagicHeader, MagicPacket> {
//      public:
//       // Initializes packet with pancakes.
//       void InitPancakes();
//     };
//
//     // Create an instance of FixedBufferPacket declared above.
//     std::unique_ptr<MagicPacket> packet =
//         std::make_unique<FixedBufferPacket<MagicHeader, 255>>();
//     packet->InitPancakes();
//
//   This pattern is used by the CommandPacket, EventPacket, and ACLDataPacket
//   classes (see control_packets.h and acl_data_packet.h).
//
// THREAD-SAFETY:
//
//   Packet is NOT thread-safe without external locking.

// PacketBase provides basic view functionality and intrusively
// linked-listability. Intended to be inherited by the Packet template and all
// of its specializations.
template <typename HeaderType, typename T>
class PacketBase : public common::LinkedListable<T> {
 public:
  virtual ~PacketBase() = default;

  const common::PacketView<HeaderType>& view() const { return view_; }
  common::MutablePacketView<HeaderType>* mutable_view() { return &view_; }

 protected:
  PacketBase() = default;

  // Called by derived classes to initialize |view_| after initializing the
  // corresponding buffer.
  void init_view(const common::MutablePacketView<HeaderType>& view) {
    ZX_DEBUG_ASSERT(!view_.is_valid());
    ZX_DEBUG_ASSERT(view.is_valid());
    view_ = view;
  }

 private:
  common::MutablePacketView<HeaderType> view_;

  FXL_DISALLOW_COPY_AND_ASSIGN(PacketBase);
};

// The basic Packet template. See control_packets.h and acl_data_packet.h for
// specializations that add functionality beyond that of PacketBase.
template <typename HeaderType>
class Packet : public PacketBase<HeaderType, Packet<HeaderType>> {
 protected:
  Packet() = default;
};

}  // namespace hci
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_PACKET_H_
