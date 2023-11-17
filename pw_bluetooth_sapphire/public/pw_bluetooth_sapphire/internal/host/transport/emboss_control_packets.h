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

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/emboss_packet.h"
#include "pw_bluetooth_sapphire/internal/host/transport/error.h"

namespace bt::hci {

template <class ViewT>
class EmbossCommandPacketT;

// EmbossCommandPacket is the HCI Command packet specialization of
// DynamicPacket.
class EmbossCommandPacket : public DynamicPacket {
 public:
  // Construct an HCI Command packet from an Emboss view T and initialize its
  // header with the |opcode| and size.
  template <typename T>
  static EmbossCommandPacketT<T> New(hci_spec::OpCode opcode) {
    return New<T>(opcode, T::IntrinsicSizeInBytes().Read());
  }

  // Construct an HCI Command packet from an Emboss view T of |packet_size|
  // total bytes (header + payload) and initialize its header with the |opcode|
  // and size. This constructor is meant for variable size packets, for which
  // clients must calculate packet size manually.
  template <typename T>
  static EmbossCommandPacketT<T> New(hci_spec::OpCode opcode,
                                     size_t packet_size) {
    EmbossCommandPacketT<T> packet(opcode, packet_size);
    return packet;
  }

  hci_spec::OpCode opcode() const;
  // Returns the OGF (OpCode Group Field) which occupies the upper 6-bits of the
  // opcode.
  uint8_t ogf() const;
  // Returns the OCF (OpCode Command Field) which occupies the lower 10-bits of
  // the opcode.
  uint16_t ocf() const;

 protected:
  explicit EmbossCommandPacket(hci_spec::OpCode opcode, size_t packet_size);

 private:
  pw::bluetooth::emboss::CommandHeaderView header_view() const;
};

// Helper subclass that remembers the view type it was constructed with. It is
// safe to slice an EmbossCommandPacketT into an EmbossCommandPacket.
template <class ViewT>
class EmbossCommandPacketT : public EmbossCommandPacket {
 public:
  ViewT view_t() { return view<ViewT>(); }

 private:
  friend class EmbossCommandPacket;

  EmbossCommandPacketT(hci_spec::OpCode opcode, size_t packet_size)
      : EmbossCommandPacket(opcode, packet_size) {}
};

template <class ViewT>
class EmbossEventPacketT;

// EmbossEventPacket is the HCI Event packet specialization of DynamicPacket.
class EmbossEventPacket : public DynamicPacket {
 public:
  // Construct an HCI Event packet of |packet_size| total bytes (header +
  // payload).
  static EmbossEventPacket New(size_t packet_size) {
    EmbossEventPacket packet(packet_size);
    return packet;
  }

  // Construct an HCI Event packet from an Emboss view T and initialize its
  // header with the |event_code| and size.
  template <typename T>
  static EmbossEventPacketT<T> New(hci_spec::EventCode event_code) {
    EmbossEventPacketT<T> packet(T::IntrinsicSizeInBytes().Read());
    auto header =
        packet.template view<pw::bluetooth::emboss::EventHeaderWriter>();
    header.event_code().Write(event_code);
    header.parameter_total_size().Write(
        T::IntrinsicSizeInBytes().Read() -
        pw::bluetooth::emboss::EventHeader::IntrinsicSizeInBytes());
    return packet;
  }

  // Construct an HCI Event packet from an Emboss view T of |packet_size| total
  // bytes (header + payload) and initialize its header with the |event_code|
  // and size. This constructor is meant for variable size packets, for which
  // clients must calculate packet size manually.
  template <typename T>
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  static EmbossEventPacketT<T> New(hci_spec::EventCode event_code,
                                   size_t packet_size) {
    EmbossEventPacketT<T> packet(packet_size);
    auto header =
        packet.template view<pw::bluetooth::emboss::EventHeaderWriter>();
    header.event_code().Write(event_code);
    header.parameter_total_size().Write(
        packet_size -
        pw::bluetooth::emboss::EventHeader::IntrinsicSizeInBytes());
    return packet;
  }

  hci_spec::EventCode event_code() const;

  // If this event packet contains a StatusCode field, this method returns the
  // status. Not all events contain a StatusCode and not all of those that do
  // are supported by this method. Returns std::nullopt for such events.
  //
  // NOTE: If you intend to use this with a new event code, make sure to add an
  // entry to the implementation in emboss_control_packets.cc.
  std::optional<pw::bluetooth::emboss::StatusCode> StatusCode() const;

  // Returns a status if this event represents the result of an operation. See
  // the documentation on StatusCode() as the same conditions apply to this
  // method. Returns a default status of type HostError::kMalformedPacket on
  // error.
  hci::Result<> ToResult() const;

 protected:
  explicit EmbossEventPacket(size_t packet_size);

 private:
  // From an Emboss view T containing a StatusCode field named "status", returns
  // the status. Returns std::nullopt on error.
  template <typename T>
  std::optional<pw::bluetooth::emboss::StatusCode> StatusCodeFromView() const {
    // Don't use view(), which asserts on IsComplete().
    T packet_view(data().data(), size());
    if (!packet_view.status().Ok()) {
      return std::nullopt;
    }
    return packet_view.status().UncheckedRead();
  }
};

// Helper subclass that remembers the view type it was constructed with. It is
// safe to slice an EmbossEventPacketT into an EmbossEventPacket.
template <class ViewT>
class EmbossEventPacketT : public EmbossEventPacket {
 public:
  template <typename... Args>
  ViewT view_t(Args... args) {
    return view<ViewT>(args...);
  }

 private:
  friend class EmbossEventPacket;

  explicit EmbossEventPacketT(size_t packet_size)
      : EmbossEventPacket(packet_size) {}
};

}  // namespace bt::hci
