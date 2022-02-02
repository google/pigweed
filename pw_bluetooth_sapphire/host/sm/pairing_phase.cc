// Copyright 2020 the Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_phase.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt::sm {

PairingPhase::PairingPhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                           Role role)
    : sm_chan_(std::move(chan)), listener_(std::move(listener)), role_(role), has_failed_(false) {}

void PairingPhase::OnFailure(Error error) {
  ZX_ASSERT(!has_failed());
  bt_log(WARN, "sm", "pairing failed: %s", bt_str(error));
  has_failed_ = true;
  ZX_ASSERT(listener_);
  listener_->OnPairingFailed(error);
}

void PairingPhase::Abort(ErrorCode ecode) {
  ZX_ASSERT(!has_failed());
  Error error = ToResult(ecode).error_value();
  bt_log(INFO, "sm", "abort pairing: %s", bt_str(error));

  sm_chan().SendMessage(kPairingFailed, ecode);
  OnFailure(error);
}

void PairingPhase::HandleChannelClosed() {
  bt_log(WARN, "sm", "channel closed while pairing");

  OnFailure(Error(HostError::kLinkDisconnected));
}
}  // namespace bt::sm
