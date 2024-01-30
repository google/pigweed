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

#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_interrogator.h"

#include "pw_bluetooth_sapphire/internal/host/gap/peer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

namespace bt::gap {

LowEnergyInterrogator::LowEnergyInterrogator(
    Peer::WeakPtr peer,
    hci_spec::ConnectionHandle handle,
    hci::CommandChannel::WeakPtr cmd_channel)
    : peer_(std::move(peer)),
      peer_id_(peer_->identifier()),
      handle_(handle),
      cmd_runner_(cmd_channel->AsWeakPtr()),
      weak_self_(this) {}

void LowEnergyInterrogator::Start(ResultCallback callback) {
  BT_ASSERT(!callback_);
  callback_ = std::move(callback);

  if (!peer_.is_alive()) {
    Complete(ToResult(HostError::kFailed));
    return;
  }

  BT_ASSERT(peer_->le().has_value());

  // Always read remote version information as a test of whether the connection
  // was *actually* successfully established. If the connection failed to be
  // established, the command status of the Read Remote Version Information
  // command will be "Connection Failed to be Established". See
  // fxbug.dev/42138706 for details.
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

  auto self = weak_self_.GetWeakPtr();

  // callback may destroy this object
  callback_(result);

  // Complete() may have been called by a command callback, in which case the
  // runner needs to be canceled.
  if (self.is_alive() && !cmd_runner_.IsReady()) {
    cmd_runner_.Cancel();
  }
}

void LowEnergyInterrogator::QueueReadLERemoteFeatures() {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LEReadRemoteFeaturesCommandWriter>(
      hci_spec::kLEReadRemoteFeatures);
  packet.view_t().connection_handle().Write(handle_);

  // It's safe to capture |this| instead of a weak ptr to self because
  // |cmd_runner_| guarantees that |cmd_cb| won't be invoked if |cmd_runner_| is
  // destroyed, and |this| outlives |cmd_runner_|.
  auto cmd_cb = [this](const hci::EmbossEventPacket& event) {
    if (hci_is_error(event, WARN, "gap-le", "LE read remote features failed")) {
      return;
    }
    bt_log(DEBUG,
           "gap-le",
           "LE read remote features complete (peer: %s)",
           bt_str(peer_id_));
    auto view = event.view<
        pw::bluetooth::emboss::LEReadRemoteFeaturesCompleteSubeventView>();
    peer_->MutLe().SetFeatures(hci_spec::LESupportedFeatures{
        view.le_features().BackingStorage().ReadUInt()});
  };

  bt_log(TRACE,
         "gap-le",
         "sending LE read remote features command (peer id: %s)",
         bt_str(peer_id_));
  cmd_runner_.QueueLeAsyncCommand(
      std::move(packet),
      hci_spec::kLEReadRemoteFeaturesCompleteSubeventCode,
      std::move(cmd_cb),
      /*wait=*/false);
}

void LowEnergyInterrogator::QueueReadRemoteVersionInformation() {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::ReadRemoteVersionInfoCommandWriter>(
      hci_spec::kReadRemoteVersionInfo);
  packet.view_t().connection_handle().Write(handle_);

  // It's safe to capture |this| instead of a weak ptr to self because
  // |cmd_runner_| guarantees that |cmd_cb| won't be invoked if |cmd_runner_| is
  // destroyed, and |this| outlives |cmd_runner_|.
  auto cmd_cb = [this](const hci::EmbossEventPacket& event) {
    if (hci_is_error(
            event, WARN, "gap-le", "read remote version info failed")) {
      return;
    }
    BT_DEBUG_ASSERT(event.event_code() ==
                    hci_spec::kReadRemoteVersionInfoCompleteEventCode);
    bt_log(TRACE,
           "gap-le",
           "read remote version info completed (peer: %s)",
           bt_str(peer_id_));
    auto view = event.view<
        pw::bluetooth::emboss::ReadRemoteVersionInfoCompleteEventView>();
    peer_->set_version(view.version().Read(),
                       view.company_identifier().Read(),
                       view.subversion().Read());
  };

  bt_log(TRACE,
         "gap-le",
         "asking for version info (peer id: %s)",
         bt_str(peer_id_));
  cmd_runner_.QueueCommand(std::move(packet),
                           std::move(cmd_cb),
                           /*wait=*/false,
                           hci_spec::kReadRemoteVersionInfoCompleteEventCode);
}

}  // namespace bt::gap
