// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_interrogator.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

namespace bt {
namespace gap {

BrEdrInterrogator::BrEdrInterrogator(PeerCache* cache, fxl::RefPtr<hci::Transport> hci,
                                     async_dispatcher_t* dispatcher)
    : Interrogator(cache, std::move(hci), dispatcher), weak_ptr_factory_(this) {}

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
  } else if (peer->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
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
  hci::PageScanRepetitionMode mode = hci::PageScanRepetitionMode::kR0;
  if (peer->bredr()->page_scan_repetition_mode()) {
    mode = *peer->bredr()->page_scan_repetition_mode();
  }
  auto packet =
      hci::CommandPacket::New(hci::kRemoteNameRequest, sizeof(hci::RemoteNameRequestCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  auto params = packet->mutable_payload<hci::RemoteNameRequestCommandParams>();
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

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_ASSERT(event.event_code() == hci::kRemoteNameRequestCompleteEventCode);

    bt_log(SPEW, "gap-bredr", "name request complete (peer id: %s)",
           bt_str(interrogation->peer_id()));

    const auto& params = event.params<hci::RemoteNameRequestCompleteEventParams>();

    size_t len = 0;
    for (; len < hci::kMaxNameLength; len++) {
      if (params.remote_name[len] == 0) {
        break;
      }
    }
    Peer* peer = self->peer_cache()->FindById(interrogation->peer_id());
    if (!peer) {
      interrogation->Complete(hci::Status(HostError::kFailed));
      return;
    }
    peer->SetName(std::string(params.remote_name, params.remote_name + len));
  };

  bt_log(SPEW, "gap-bredr", "sending name request (peer id: %s)", bt_str(interrogation->peer_id()));
  hci()->command_channel()->SendExclusiveCommand(std::move(packet), dispatcher(), std::move(cmd_cb),
                                                 hci::kRemoteNameRequestCompleteEventCode,
                                                 {hci::kInquiry});
}

void BrEdrInterrogator::ReadRemoteFeatures(InterrogationRefPtr interrogation) {
  auto packet = hci::CommandPacket::New(hci::kReadRemoteSupportedFeatures,
                                        sizeof(hci::ReadRemoteSupportedFeaturesCommandParams));
  packet->mutable_payload<hci::ReadRemoteSupportedFeaturesCommandParams>()->connection_handle =
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

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_ASSERT(event.event_code() == hci::kReadRemoteSupportedFeaturesCompleteEventCode);

    bt_log(SPEW, "gap-bredr", "remote features request complete (peer id: %s)",
           bt_str(interrogation->peer_id()));

    const auto& params =
        event.view().payload<hci::ReadRemoteSupportedFeaturesCompleteEventParams>();

    Peer* peer = self->peer_cache()->FindById(interrogation->peer_id());
    if (!peer) {
      interrogation->Complete(hci::Status(HostError::kFailed));
      return;
    }
    peer->SetFeaturePage(0, le64toh(params.lmp_features));

    if (peer->features().HasBit(0, hci::LMPFeature::kExtendedFeatures)) {
      peer->set_last_page_number(1);
      self->ReadRemoteExtendedFeatures(interrogation, 1);
    }
  };

  bt_log(SPEW, "gap-bredr", "asking for supported features (peer id: %s)",
         bt_str(interrogation->peer_id()));
  hci()->command_channel()->SendCommand(std::move(packet), dispatcher(), std::move(cmd_cb),
                                        hci::kReadRemoteSupportedFeaturesCompleteEventCode);
}

void BrEdrInterrogator::ReadRemoteExtendedFeatures(InterrogationRefPtr interrogation,
                                                   uint8_t page) {
  auto packet = hci::CommandPacket::New(hci::kReadRemoteExtendedFeatures,
                                        sizeof(hci::ReadRemoteExtendedFeaturesCommandParams));
  auto params = packet->mutable_payload<hci::ReadRemoteExtendedFeaturesCommandParams>();
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

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_ASSERT(event.event_code() == hci::kReadRemoteExtendedFeaturesCompleteEventCode);

    const auto& params =
        event.view().template payload<hci::ReadRemoteExtendedFeaturesCompleteEventParams>();

    Peer* peer = self->peer_cache()->FindById(interrogation->peer_id());
    if (!peer) {
      interrogation->Complete(hci::Status(HostError::kFailed));
      return;
    }

    bt_log(SPEW, "gap-bredr",
           "got extended features page %u, max page %u (requested page: %u, peer id: %s)",
           params.page_number, params.max_page_number, page, bt_str(interrogation->peer_id()));

    peer->SetFeaturePage(params.page_number, le64toh(params.lmp_features));

    if (params.page_number != page) {
      bt_log(INFO, "gap-bredr", "requested page %u and got page %u, giving up", page,
             params.page_number);
      peer->set_last_page_number(0);
      return;
    }

    // NOTE: last page number will be capped at 2
    peer->set_last_page_number(params.max_page_number);

    if (page < peer->features().last_page_number()) {
      self->ReadRemoteExtendedFeatures(interrogation, page + 1);
    }
  };

  bt_log(SPEW, "gap-bredr", "requesting extended features page %u (peer id: %s)", page,
         bt_str(interrogation->peer_id()));
  hci()->command_channel()->SendCommand(std::move(packet), dispatcher(), std::move(cmd_cb),
                                        hci::kReadRemoteExtendedFeaturesCompleteEventCode);
}  // namespace gap

}  // namespace gap
}  // namespace bt
