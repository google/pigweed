// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_CONTROL_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_CONTROL_PACKETS_H_

#include <endian.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/packet.h"

namespace bt::hci {

using CommandPacket = Packet<hci_spec::CommandHeader>;
using EventPacket = Packet<hci_spec::EventHeader>;
using EventPacketPtr = std::unique_ptr<EventPacket>;

// Packet template specialization for HCI command packets.
template <>
class Packet<hci_spec::CommandHeader> : public PacketBase<hci_spec::CommandHeader, CommandPacket> {
 public:
  // Slab-allocates a new CommandPacket with the given payload size and
  // initializes the packet's header field.
  static std::unique_ptr<CommandPacket> New(hci_spec::OpCode opcode, size_t payload_size = 0u);

  // Returns the HCI command opcode currently in this packet.
  hci_spec::OpCode opcode() const { return le16toh(view().header().opcode); }

  // Convenience function to get a mutable payload of a packet.
  template <typename PayloadType>
  PayloadType* mutable_payload() {
    return mutable_view()->mutable_payload<PayloadType>();
  }

 protected:
  using PacketBase<hci_spec::CommandHeader, CommandPacket>::PacketBase;

 private:
  // Writes the given header fields into the underlying buffer.
  void WriteHeader(hci_spec::OpCode opcode);
};

// Packet template specialization for HCI event packets.
template <>
class Packet<hci_spec::EventHeader> : public PacketBase<hci_spec::EventHeader, EventPacket> {
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

  // If this is a CommandComplete event packet, this method returns a pointer to
  // the beginning of the return parameter structure. If the given template type
  // would exceed the bounds of the packet or if this packet does not represent
  // a CommandComplete event, this method returns nullptr.
  template <typename ReturnParams>
  const ReturnParams* return_params() const {
    if (event_code() != hci_spec::kCommandCompleteEventCode ||
        sizeof(ReturnParams) > view().payload_size() - sizeof(hci_spec::CommandCompleteEventParams))
      return nullptr;
    return reinterpret_cast<const ReturnParams*>(
        params<hci_spec::CommandCompleteEventParams>().return_parameters);
  }

  // If this is a LE Meta Event or Vendor Event packet, this method returns a pointer to the
  // beginning of the subevent parameter structure. If the given template type would exceed the
  // bounds of the packet or if this packet does not represent a LE Meta Event or a Vendor Event,
  // this method returns nullptr.
  template <typename SubeventParams>
  const SubeventParams* subevent_params() const {
    hci_spec::EventCode ev_code = event_code();
    switch (ev_code) {
      case hci_spec::kLEMetaEventCode:
        if (sizeof(SubeventParams) > view().payload_size() - sizeof(hci_spec::LEMetaEventParams)) {
          return nullptr;
        }

        return reinterpret_cast<const SubeventParams*>(
            params<hci_spec::LEMetaEventParams>().subevent_parameters);
      case hci_spec::kVendorDebugEventCode:
        if (sizeof(SubeventParams) > view().payload_size() - sizeof(hci_spec::VendorEventParams)) {
          return nullptr;
        }

        return reinterpret_cast<const SubeventParams*>(
            params<hci_spec::VendorEventParams>().subevent_parameters);
      default:
        return nullptr;
    }
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
  bool ToStatusCode(hci_spec::StatusCode* out_code) const;

  // Returns a status if this event represents the result of an operation. See
  // the documentation on Tohci_spec::StatusCode() as the same conditions apply to this
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
#define hci_is_error(event, flag, tag, fmt...) bt_is_error(event.ToResult(), flag, tag, fmt)

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_CONTROL_PACKETS_H_
