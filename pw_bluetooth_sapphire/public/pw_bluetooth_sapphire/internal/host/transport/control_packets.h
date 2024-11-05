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

#include <pw_bytes/endian.h>

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/error.h"
#include "pw_bluetooth_sapphire/internal/host/transport/packet.h"

namespace bt::hci {

using EventPacket = Packet<hci_spec::EventHeader>;
using EventPacketPtr = std::unique_ptr<EventPacket>;

// Packet template specialization for HCI event packets.
template <>
class Packet<hci_spec::EventHeader>
    : public PacketBase<hci_spec::EventHeader, EventPacket> {
 public:
  // Slab-allocates a new EventPacket with the given payload size without
  // initializing its contents.
  static std::unique_ptr<EventPacket> New(size_t payload_size);

  // Returns the HCI event code currently in this packet.
  hci_spec::EventCode event_code() const { return view().header().event_code; }

  // Convenience function to get a parameter payload from a packet
  template <typename ParamsType>
  const ParamsType& params() const {
    return view().payload<ParamsType>();
  }

  // If this is a LE Meta Event packet, this method returns a pointer to the
  // beginning of the subevent parameter structure. If the given template type
  // would exceed the bounds of the packet or if this packet does not represent
  // a LE Meta Event, this method returns nullptr.
  template <typename SubeventParams>
  const SubeventParams* subevent_params() const {
    if (event_code() != hci_spec::kLEMetaEventCode ||
        sizeof(SubeventParams) >
            view().payload_size() - sizeof(hci_spec::LEMetaEventParams)) {
      return nullptr;
    }

    return reinterpret_cast<const SubeventParams*>(
        params<hci_spec::LEMetaEventParams>().subevent_parameters);
  }

  // If this is an event packet with a standard status (See Vol 2, Part D), this
  // method returns true and populates |out_status| using the status from the
  // event parameters.
  //
  // Not all events contain a status code and not all of those that do are
  // supported by this method. Returns false for such events and |out_status| is
  // left unmodified.
  //
  // NOTE: Using this method on an unsupported event packet will trigger an
  // assertion in debug builds. If you intend to use this with a new event code,
  // make sure to add an entry to the implementation in control_packets.cc.
  //
  // TODO(armansito): Add more event entries here as needed.
  bool ToStatusCode(pw::bluetooth::emboss::StatusCode* out_code) const;

  // Returns a status if this event represents the result of an operation. See
  // the documentation on ToStatusCode() as the same conditions apply to this
  // method. Instead of a boolean, this returns a default status of type
  // HostError::kMalformedPacket.
  Result<> ToResult() const;

  // Initializes the internal PacketView by reading the header portion of the
  // underlying buffer.
  void InitializeFromBuffer();

 protected:
  using PacketBase<hci_spec::EventHeader, EventPacket>::PacketBase;
};

}  // namespace bt::hci

// Convenience macros to check and log any non-Success status of an event.
// Evaluate to true if the event status is not success.
#define hci_is_error(event, flag, tag, /*fmt*/...) \
  bt_is_error(event.ToResult(), flag, tag, __VA_ARGS__)
