// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_DATA_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_DATA_PACKET_H_

#include <fbl/macros.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/packet.h"

namespace bt {
namespace hci {

// Packet template specialization for ACL data packets. This cannot be directly
// instantiated. Represents a HCI ACL data packet.
using ACLDataPacket = Packet<ACLDataHeader>;
using ACLDataPacketPtr = std::unique_ptr<ACLDataPacket>;

template <>
class Packet<ACLDataHeader> : public PacketBase<ACLDataHeader, ACLDataPacket> {
 public:
  // Slab-allocates a new ACLDataPacket with the given payload size without
  // initializing its contents.
  static ACLDataPacketPtr New(uint16_t payload_size);

  // Slab-allocates a new ACLDataPacket with the given payload size and
  // initializes the packet's header field with the given data.
  static ACLDataPacketPtr New(ConnectionHandle connection_handle,
                              ACLPacketBoundaryFlag packet_boundary_flag,
                              ACLBroadcastFlag broadcast_flag, uint16_t payload_size = 0u);

  // Getters for the header fields.
  ConnectionHandle connection_handle() const;
  ACLPacketBoundaryFlag packet_boundary_flag() const;
  ACLBroadcastFlag broadcast_flag() const;

  // Initializes the internal PacketView by reading the header portion of the
  // underlying buffer.
  void InitializeFromBuffer();

 protected:
  Packet<EventHeader>() = default;

 private:
  // Writes the given header fields into the underlying buffer.
  void WriteHeader(ConnectionHandle connection_handle, ACLPacketBoundaryFlag packet_boundary_flag,
                   ACLBroadcastFlag broadcast_flag);
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_DATA_PACKET_H_
