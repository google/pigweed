// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"

#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h"

#include <pw_bluetooth/hci_vendor.emb.h>

namespace bt::hci {

EmbossCommandPacket::EmbossCommandPacket(hci_spec::OpCode opcode, size_t packet_size)
    : DynamicPacket(packet_size) {
  BT_ASSERT_MSG(packet_size >= pw::bluetooth::emboss::CommandHeader::IntrinsicSizeInBytes(),
                "command packet size must be at least 3 bytes to accomodate header");
  auto header = view<pw::bluetooth::emboss::CommandHeaderWriter>();
  header.opcode().BackingStorage().WriteUInt(opcode);
  header.parameter_total_size().Write(packet_size -
                                      pw::bluetooth::emboss::CommandHeader::IntrinsicSizeInBytes());
}

hci_spec::OpCode EmbossCommandPacket::opcode() const {
  return header_view().opcode().BackingStorage().ReadUInt();
}

uint8_t EmbossCommandPacket::ogf() const { return header_view().opcode().ogf().Read(); }

uint16_t EmbossCommandPacket::ocf() const { return header_view().opcode().ocf().Read(); }

pw::bluetooth::emboss::CommandHeaderView EmbossCommandPacket::header_view() const {
  return view<pw::bluetooth::emboss::CommandHeaderView>();
}

EmbossEventPacket::EmbossEventPacket(size_t packet_size) : DynamicPacket(packet_size) {
  BT_ASSERT_MSG(packet_size >= pw::bluetooth::emboss::EventHeader::IntrinsicSizeInBytes(),
                "event packet size must be at least 2 bytes to accomodate header");
}

hci_spec::EventCode EmbossEventPacket::event_code() const {
  return view<pw::bluetooth::emboss::EventHeaderView>().event_code().Read();
}

std::optional<pw::bluetooth::emboss::StatusCode> EmbossEventPacket::StatusCode() const {
  switch (event_code()) {
    case hci_spec::kCommandCompleteEventCode:
      return StatusCodeFromView<pw::bluetooth::emboss::SimpleCommandCompleteEventView>();
    case hci_spec ::kCommandStatusEventCode:
      return StatusCodeFromView<pw::bluetooth::emboss::CommandStatusEventView>();
    case hci_spec::kConnectionCompleteEventCode:
      return StatusCodeFromView<pw::bluetooth::emboss::ConnectionCompleteEventView>();
    case hci_spec::kDisconnectionCompleteEventCode:
      return StatusCodeFromView<pw::bluetooth::emboss::DisconnectionCompleteEventView>();
    case hci_spec::kReadRemoteVersionInfoCompleteEventCode:
      return StatusCodeFromView<pw::bluetooth::emboss::ReadRemoteVersionInfoCompleteEventView>();
    case hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode:
      return StatusCodeFromView<
          pw::bluetooth::emboss::ReadRemoteSupportedFeaturesCompleteEventView>();
    case hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode:
      return StatusCodeFromView<
          pw::bluetooth::emboss::ReadRemoteExtendedFeaturesCompleteEventView>();
    case hci_spec::kRemoteNameRequestCompleteEventCode: {
      // Tests expect that a kPacketMalformed status is returned for incomplete events, even if they
      // contain the status field.
      pw::bluetooth::emboss::RemoteNameRequestCompleteEventView event_view(data().data(), size());
      if (!event_view.IsComplete()) {
        return std::nullopt;
      }
      return event_view.status().UncheckedRead();
    }
    case hci_spec::kEncryptionChangeEventCode:
      return StatusCodeFromView<pw::bluetooth::emboss::EncryptionChangeEventV1View>();
    case hci_spec::kVendorDebugEventCode: {
      hci_spec::EventCode subevent_code =
          view<pw::bluetooth::emboss::VendorDebugEventView>().subevent_code().Read();

      switch (subevent_code) {
        case hci_spec::vendor::android::kLEMultiAdvtStateChangeSubeventCode: {
          return StatusCodeFromView<
              pw::bluetooth::vendor::android_hci::LEMultiAdvtStateChangeSubeventView>();
        }

        default: {
          BT_PANIC("Emboss vendor subevent (%#.2x) not implemented", subevent_code);
          break;
        }
      }

      break;
    }

    case hci_spec::kLEMetaEventCode: {
      hci_spec::EventCode subevent_code =
          view<pw::bluetooth::emboss::LEMetaEventView>().subevent_code().Read();

      switch (subevent_code) {
        case hci_spec::kLEConnectionCompleteSubeventCode: {
          return StatusCodeFromView<pw::bluetooth::emboss::LEConnectionCompleteSubeventView>();
        }

        case hci_spec::kLEConnectionUpdateCompleteSubeventCode: {
          return StatusCodeFromView<
              pw::bluetooth::emboss::LEConnectionUpdateCompleteSubeventView>();
        }

        case hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode: {
          return StatusCodeFromView<
              pw::bluetooth::emboss::LEReadRemoteFeaturesCompleteSubeventView>();
        }

        default: {
          BT_PANIC("Emboss LE meta subevent (%#.2x) not implemented", subevent_code);
          break;
        }
      }

      break;
    }

    default: {
      BT_PANIC("Emboss event (%#.2x) not implemented", event_code());
      break;
    }
  }

  return std::nullopt;
}

hci::Result<> EmbossEventPacket::ToResult() const {
  std::optional<pw::bluetooth::emboss::StatusCode> maybe_status_code = StatusCode();
  if (!maybe_status_code.has_value()) {
    return bt::ToResult(HostError::kPacketMalformed);
  }
  return bt::ToResult(*maybe_status_code);
}

}  // namespace bt::hci
