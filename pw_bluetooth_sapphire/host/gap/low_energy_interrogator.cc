// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_interrogator.h"

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

namespace bt::gap {

LowEnergyInterrogator::LowEnergyInterrogator(PeerCache* cache, fxl::WeakPtr<hci::Transport> hci)
    : Interrogator(cache, std::move(hci)), weak_ptr_factory_(this) {}

void LowEnergyInterrogator::SendCommands(InterrogationRefPtr interrogation) {
  Peer* peer = peer_cache()->FindById(interrogation->peer_id());
  if (!peer) {
    interrogation->Complete(hci::Status(HostError::kFailed));
    return;
  }

  ZX_ASSERT(peer->le().has_value());

  // Always read remote version information as a test of whether the connection was *actually*
  // successfully established. If the connection failed to be established, the command status of the
  // Read Remote Version Information command will be "Connection Failed to be Established". See
  // fxbug.dev/60517 for details.
  ReadRemoteVersionInformation(interrogation);

  if (!peer->le()->features().has_value()) {
    ReadLERemoteFeatures(interrogation);
  }
}

void LowEnergyInterrogator::ReadLERemoteFeatures(InterrogationRefPtr interrogation) {
  auto packet = hci::CommandPacket::New(hci::kLEReadRemoteFeatures,
                                        sizeof(hci::LEReadRemoteFeaturesCommandParams));
  auto params = packet->mutable_payload<hci::LEReadRemoteFeaturesCommandParams>();
  params->connection_handle = htole16(interrogation->handle());

  auto cmd_cb = [interrogation, self = weak_ptr_factory_.GetWeakPtr()](
                    auto id, const hci::EventPacket& event) {
    if (!self) {
      return;
    }

    if (!interrogation->active()) {
      return;
    }

    if (hci_is_error(event, WARN, "gap-le", "LE read remote features failed")) {
      interrogation->Complete(event.ToStatus());
      return;
    }

    if (event.event_code() == hci::kCommandStatusEventCode) {
      return;
    }

    ZX_ASSERT(event.event_code() == hci::kLEMetaEventCode);

    bt_log(DEBUG, "gap-le", "LE read remote features complete (peer: %s)",
           bt_str(interrogation->peer_id()));

    const auto* params = event.le_event_params<hci::LEReadRemoteFeaturesCompleteSubeventParams>();

    Peer* peer = self->peer_cache()->FindById(interrogation->peer_id());
    if (!peer) {
      interrogation->Complete(hci::Status(HostError::kFailed));
      return;
    }

    peer->MutLe().SetFeatures(hci::LESupportedFeatures{params->le_features});
  };

  bt_log(TRACE, "gap-le", "sending LE read remote features command (peer id: %s)",
         bt_str(interrogation->peer_id()));
  hci()->command_channel()->SendLeAsyncCommand(std::move(packet), std::move(cmd_cb),
                                               hci::kLEReadRemoteFeaturesCompleteSubeventCode);
}

}  // namespace bt::gap
