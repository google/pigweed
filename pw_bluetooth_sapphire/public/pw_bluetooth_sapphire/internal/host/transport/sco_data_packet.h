// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SCO_DATA_PACKET_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SCO_DATA_PACKET_H_

#include <lib/fit/function.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/packet.h"

namespace bt::hci {

// Packet template specialization for SCO data packets. ScoDataPacket does not
// have a public ctor, so clients should use its |New| factory methods to
// instantiate it.
using ScoDataPacket = Packet<hci_spec::SynchronousDataHeader>;
using ScoPacketHandler =
    fit::function<void(std::unique_ptr<ScoDataPacket> data_packet)>;

template <>
class Packet<hci_spec::SynchronousDataHeader>
    : public PacketBase<hci_spec::SynchronousDataHeader, ScoDataPacket> {
 public:
  // Slab-allocates a new ScoDataPacket with the given payload size without
  // initializing its contents.
  static std::unique_ptr<ScoDataPacket> New(uint8_t payload_size);

  // Slab-allocates a new ScoDataPacket with the given payload size and
  // initializes the packet's header field with the given data.
  static std::unique_ptr<ScoDataPacket> New(
      hci_spec::ConnectionHandle connection_handle, uint8_t payload_size);

  // Getters for the header fields.
  hci_spec::ConnectionHandle connection_handle() const;
  hci_spec::SynchronousDataPacketStatusFlag packet_status_flag() const;

  // Initializes the internal PacketView by reading the header portion of the
  // underlying buffer.
  void InitializeFromBuffer();

 protected:
  using PacketBase<hci_spec::SynchronousDataHeader, ScoDataPacket>::PacketBase;

 private:
  // Writes the given header fields into the underlying buffer.
  void WriteHeader(hci_spec::ConnectionHandle connection_handle);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_SCO_DATA_PACKET_H_
