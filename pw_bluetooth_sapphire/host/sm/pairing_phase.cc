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

#include "pw_bluetooth_sapphire/internal/host/sm/pairing_phase.h"

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::sm {

PairingPhase::PairingPhase(PairingChannel::WeakPtr chan,
                           Listener::WeakPtr listener,
                           Role role)
    : sm_chan_(std::move(chan)),
      listener_(std::move(listener)),
      role_(role),
      has_failed_(false),
      weak_channel_handler_(nullptr) {}

void PairingPhase::SetPairingChannelHandler(PairingChannelHandler& self) {
  weak_channel_handler_ = WeakSelf(&self);
  sm_chan().SetChannelHandler(weak_channel_handler_.GetWeakPtr());
}

void PairingPhase::InvalidatePairingChannelHandler() {
  weak_channel_handler_.InvalidatePtrs();
}

void PairingPhase::OnFailure(Error error) {
  BT_ASSERT(!has_failed());
  bt_log(WARN, "sm", "pairing failed: %s", bt_str(error));
  has_failed_ = true;
  BT_ASSERT(listener_.is_alive());
  listener_->OnPairingFailed(error);
}

void PairingPhase::Abort(ErrorCode ecode) {
  BT_ASSERT(!has_failed());
  Error error(ecode);
  bt_log(INFO, "sm", "abort pairing: %s", bt_str(error));

  sm_chan().SendMessage(kPairingFailed, ecode);
  OnFailure(error);
}

void PairingPhase::HandleChannelClosed() {
  bt_log(WARN, "sm", "channel closed while pairing");

  OnFailure(Error(HostError::kLinkDisconnected));
}
}  // namespace bt::sm
