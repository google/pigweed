// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_interrogator.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::gap {

BrEdrInterrogator::BrEdrInterrogator(PeerCache* cache, fxl::WeakPtr<hci::Transport> hci)
    : Interrogator(cache, std::move(hci)), weak_ptr_factory_(this) {}

void BrEdrInterrogator::SendCommands(InterrogationRefPtr interrogation) {
  Peer* peer = peer_cache()->FindById(interrogation->peer_id());
  if (!peer) {
    interrogation->Complete(hci::Status(HostError::kFailed));
    return;
  }

  if (!peer->name()) {
    MakeRemoteNameRequest(interrogation);
  }

  if (!peer->version()) {
    ReadRemoteVersionInformation(interrogation);
  }

  if (!peer->features().HasPage(0)) {
    ReadRemoteFeatures(interrogation);
  } else if (peer->features().HasBit(0, hci_spec::LMPFeature::kExtendedFeatures)) {
    ReadRemoteExtendedFeatures(interrogation, 1);
  }
}

void BrEdrInterrogator::MakeRemoteNameRequest(InterrogationRefPtr interrogation) {
  Peer* peer = peer_cache()->FindById(interrogation->peer_id());
  if (!peer) {
    interrogation->Complete(hci::Status(HostError::kFailed));
    return;
  }
  ZX_ASSERT(peer->bredr());
  hci_spec::PageScanRepetitionMode mode = hci_spec::PageScanRepetitionMode::kR0;
  if (peer->bredr()->page_scan_repetition_mode()) {
    mode = *peer->bredr()->page_scan_repetition_mode();
  }
  auto packet = hci::CommandPacket::New(hci_spec::kRemoteNameRequest,
                                        sizeof(hci_spec::RemoteNameRequestCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto params = packet->mutable_payload<hci_spec::RemoteNameRequestCommandParams>();
  params->bd_addr = peer->address().value();
  params->page_scan_repetition_mode = mode;
  if (peer->bredr()->clock_offset()) {
    params->clock_offset = *(peer->bredr()->clock_offset());
  }

  auto cmd_cb = [interrogation, self = weak_ptr_factory_.GetWeakPtr()](
                    auto id, const hci::EventPacket& event) {
    if (!interrogation->active()) {
      return;
    }

    if (hci_is_error(event, WARN, "gap-bredr", "remote name request failed")) {
      interrogation->Complete(event.ToStatus());
      return;
    }

    if (event.event_code() == hci_spec::kCommandStatusEventCode) {
      return;
    }

    ZX_ASSERT(event.event_code() == hci_spec::kRemoteNameRequestCompleteEventCode);

    bt_log(TRACE, "gap-bredr", "name request complete (peer id: %s)",
           bt_str(interrogation->peer_id()));

    const auto& params = event.params<hci_spec::RemoteNameRequestCompleteEventParams>();

    Peer* const peer = self->peer_cache()->FindById(interrogation->peer_id());
    if (!peer) {
      interrogation->Complete(hci::Status(HostError::kFailed));
      return;
    }
    const auto remote_name_end = std::find(params.remote_name, std::end(params.remote_name), '\0');
    peer->SetName(std::string(params.remote_name, remote_name_end));
  };

  bt_log(TRACE, "gap-bredr", "sending name request (peer id: %s)",
         bt_str(interrogation->peer_id()));
  hci()->command_channel()->SendExclusiveCommand(std::move(packet), std::move(cmd_cb),
                                                 hci_spec::kRemoteNameRequestCompleteEventCode,
                                                 {hci_spec::kInquiry});
}

void BrEdrInterrogator::ReadRemoteFeatures(InterrogationRefPtr interrogation) {
  auto packet = hci::CommandPacket::New(hci_spec::kReadRemoteSupportedFeatures,
                                        sizeof(hci_spec::ReadRemoteSupportedFeaturesCommandParams));
  packet->mutable_payload<hci_spec::ReadRemoteSupportedFeaturesCommandParams>()->connection_handle =
      htole16(interrogation->handle());

  auto cmd_cb = [interrogation, self = weak_ptr_factory_.GetWeakPtr()](
                    auto id, const hci::EventPacket& event) {
    if (!interrogation->active()) {
      return;
    }

    if (hci_is_error(event, WARN, "gap-bredr", "read remote supported features failed")) {
      interrogation->Complete(event.ToStatus());
      return;
    }

    if (event.event_code() == hci_spec::kCommandStatusEventCode) {
      return;
    }

    ZX_ASSERT(event.event_code() == hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode);

    bt_log(TRACE, "gap-bredr", "remote features request complete (peer id: %s)",
           bt_str(interrogation->peer_id()));

    const auto& params =
        event.view().payload<hci_spec::ReadRemoteSupportedFeaturesCompleteEventParams>();

    Peer* peer = self->peer_cache()->FindById(interrogation->peer_id());
    if (!peer) {
      interrogation->Complete(hci::Status(HostError::kFailed));
      return;
    }
    peer->SetFeaturePage(0, le64toh(params.lmp_features));

    if (peer->features().HasBit(0, hci_spec::LMPFeature::kExtendedFeatures)) {
      peer->set_last_page_number(1);
      self->ReadRemoteExtendedFeatures(interrogation, 1);
    }
  };

  bt_log(TRACE, "gap-bredr", "asking for supported features (peer id: %s)",
         bt_str(interrogation->peer_id()));
  hci()->command_channel()->SendCommand(std::move(packet), std::move(cmd_cb),
                                        hci_spec::kReadRemoteSupportedFeaturesCompleteEventCode);
}

void BrEdrInterrogator::ReadRemoteExtendedFeatures(InterrogationRefPtr interrogation,
                                                   uint8_t page) {
  auto packet = hci::CommandPacket::New(hci_spec::kReadRemoteExtendedFeatures,
                                        sizeof(hci_spec::ReadRemoteExtendedFeaturesCommandParams));
  auto params = packet->mutable_payload<hci_spec::ReadRemoteExtendedFeaturesCommandParams>();
  params->connection_handle = htole16(interrogation->handle());
  params->page_number = page;

  auto cmd_cb = [interrogation, page, self = weak_ptr_factory_.GetWeakPtr()](auto,
                                                                             const auto& event) {
    if (!interrogation->active()) {
      return;
    }

    if (hci_is_error(event, WARN, "gap-bredr", "read remote extended features failed (peer id: %s)",
                     bt_str(interrogation->peer_id()))) {
      interrogation->Complete(event.ToStatus());
      return;
    }

    if (event.event_code() == hci_spec::kCommandStatusEventCode) {
      return;
    }

    ZX_ASSERT(event.event_code() == hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode);

    const auto& params =
        event.view().template payload<hci_spec::ReadRemoteExtendedFeaturesCompleteEventParams>();

    Peer* peer = self->peer_cache()->FindById(interrogation->peer_id());
    if (!peer) {
      interrogation->Complete(hci::Status(HostError::kFailed));
      return;
    }

    bt_log(TRACE, "gap-bredr",
           "got extended features page %u, max page %u (requested page: %u, peer id: %s)",
           params.page_number, params.max_page_number, page, bt_str(interrogation->peer_id()));

    peer->SetFeaturePage(params.page_number, le64toh(params.lmp_features));

    if (params.page_number != page) {
      bt_log(INFO, "gap-bredr", "requested page %u and got page %u, giving up (peer: %s)", page,
             params.page_number, bt_str(interrogation->peer_id()));
      peer->set_last_page_number(0);
      return;
    }

    // NOTE: last page number will be capped at 2
    peer->set_last_page_number(params.max_page_number);

    if (page < peer->features().last_page_number()) {
      self->ReadRemoteExtendedFeatures(interrogation, page + 1);
    }
  };

  bt_log(TRACE, "gap-bredr", "requesting extended features page %u (peer id: %s)", page,
         bt_str(interrogation->peer_id()));
  hci()->command_channel()->SendCommand(std::move(packet), std::move(cmd_cb),
                                        hci_spec::kReadRemoteExtendedFeaturesCompleteEventCode);
}  // namespace gap

}  // namespace bt::gap
