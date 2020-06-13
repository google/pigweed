// Copyright 2020 the Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "active_phase.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt {

namespace sm {

ActivePhase::ActivePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                         Role role)
    : PairingPhase(std::move(chan), std::move(listener), role), has_failed_(false) {}

void ActivePhase::OnFailure(Status status) {
  ZX_ASSERT(!has_failed());
  bt_log(WARN, "sm", "pairing failed: %s", bt_str(status));
  has_failed_ = true;
  ZX_ASSERT(listener());
  listener()->OnPairingFailed(status);
}

void ActivePhase::Abort(ErrorCode ecode) {
  ZX_ASSERT(!has_failed());
  Status status(ecode);
  bt_log(INFO, "sm", "abort pairing: %s", bt_str(status));

  sm_chan().SendMessage(kPairingFailed, ecode);
  OnFailure(status);
}

void ActivePhase::OnPairingTimeout() {
  ZX_ASSERT(!has_failed());
  // Pairing is no longer allowed. Disconnect the link.
  bt_log(WARN, "sm", "pairing timed out! disconnecting link");
  sm_chan()->SignalLinkError();

  OnFailure(Status(HostError::kTimedOut));
}

void ActivePhase::HandleChannelClosed() {
  bt_log(WARN, "sm", "channel closed while pairing");

  OnFailure(Status(HostError::kLinkDisconnected));
}
}  // namespace sm
}  // namespace bt
