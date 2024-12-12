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
