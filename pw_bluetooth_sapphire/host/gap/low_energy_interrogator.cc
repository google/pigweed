// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_interrogator.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::gap {

LowEnergyInterrogator::LowEnergyInterrogator(fxl::WeakPtr<Peer> peer,
                                             hci_spec::ConnectionHandle handle,
                                             fxl::WeakPtr<hci::Transport> hci)
    : hci_(std::move(hci)),
      peer_(std::move(peer)),
      peer_id_(peer_->identifier()),
      handle_(handle),
      cmd_runner_(async_get_default_dispatcher(), hci_),
      weak_ptr_factory_(this) {}

void LowEnergyInterrogator::Start(ResultCallback callback) {
  ZX_ASSERT(!callback_);
  callback_ = std::move(callback);

  if (!peer_) {
    Complete(ToResult(HostError::kFailed));
    return;
  }

  ZX_ASSERT(peer_->le().has_value());

  // Always read remote version information as a test of whether the connection was *actually*
  // successfully established. If the connection failed to be established, the command status of the
  // Read Remote Version Information command will be "Connection Failed to be Established". See
  // fxbug.dev/60517 for details.
  QueueReadRemoteVersionInformation();

  if (!peer_->le()->features().has_value()) {
    QueueReadLERemoteFeatures();
  }

  cmd_runner_.RunCommands([this](hci::Result<> result) { Complete(result); });
}

void LowEnergyInterrogator::Cancel() {
  if (!cmd_runner_.IsReady()) {
    cmd_runner_.Cancel();
  }
}

void LowEnergyInterrogator::Complete(hci::Result<> result) {
  if (!callback_) {
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();

  // callback may destroy this object
  callback_(result);

  // Complete() may have been called by a command callback, in which case the runner needs to be
  // canceled.
  if (self && !cmd_runner_.IsReady()) {
    cmd_runner_.Cancel();
  }
}

void LowEnergyInterrogator::QueueReadLERemoteFeatures() {
  auto packet = hci::CommandPacket::New(hci_spec::kLEReadRemoteFeatures,
                                        sizeof(hci_spec::LEReadRemoteFeaturesCommandParams));
  auto params = packet->mutable_payload<hci_spec::LEReadRemoteFeaturesCommandParams>();
  params->connection_handle = htole16(handle_);

  // It's safe to capture |this| instead of a weak ptr to self because |cmd_runner_| guarantees that
  // |cmd_cb| won't be invoked if |cmd_runner_| is destroyed, and |this| outlives |cmd_runner_|.
  auto cmd_cb = [this](const hci::EventPacket& event) {
    if (hci_is_error(event, WARN, "gap-le", "LE read remote features failed")) {
      return;
    }
    bt_log(DEBUG, "gap-le", "LE read remote features complete (peer: %s)", bt_str(peer_id_));
    const auto* params =
        event.subevent_params<hci_spec::LEReadRemoteFeaturesCompleteSubeventParams>();
    peer_->MutLe().SetFeatures(hci_spec::LESupportedFeatures{params->le_features});
  };

  bt_log(TRACE, "gap-le", "sending LE read remote features command (peer id: %s)",
         bt_str(peer_id_));
  cmd_runner_.QueueLeAsyncCommand(std::move(packet),
                                  hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
                                  std::move(cmd_cb), /*wait=*/false);
}

void LowEnergyInterrogator::QueueReadRemoteVersionInformation() {
  auto packet = hci::CommandPacket::New(hci_spec::kReadRemoteVersionInfo,
                                        sizeof(hci_spec::ReadRemoteVersionInfoCommandParams));
  packet->mutable_payload<hci_spec::ReadRemoteVersionInfoCommandParams>()->connection_handle =
      htole16(handle_);

  // It's safe to capture |this| instead of a weak ptr to self because |cmd_runner_| guarantees that
  // |cmd_cb| won't be invoked if |cmd_runner_| is destroyed, and |this| outlives |cmd_runner_|.
  auto cmd_cb = [this](const hci::EventPacket& event) {
    if (hci_is_error(event, WARN, "gap-le", "read remote version info failed")) {
      return;
    }
    bt_log(TRACE, "gap-le", "read remote version info completed (peer: %s)", bt_str(peer_id_));
    const auto params = event.params<hci_spec::ReadRemoteVersionInfoCompleteEventParams>();
    peer_->set_version(params.lmp_version, params.manufacturer_name, params.lmp_subversion);
  };

  bt_log(TRACE, "gap-le", "asking for version info (peer id: %s)", bt_str(peer_id_));
  cmd_runner_.QueueCommand(std::move(packet), std::move(cmd_cb), /*wait=*/false,
                           hci_spec::kReadRemoteVersionInfoCompleteEventCode);
}

}  // namespace bt::gap
