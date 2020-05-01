// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_channel.h"

#include <zircon/assert.h>

#include "lib/async/default.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {

PairingChannel::PairingChannel(fbl::RefPtr<l2cap::Channel> chan)
    : chan_(std::move(chan)), weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(chan_);
  auto self = weak_ptr_factory_.GetWeakPtr();
  chan_->ActivateWithDispatcher(
      [self](ByteBufferPtr sdu) {
        if (self) {
          self->OnRxBFrame(std::move(sdu));
        } else {
          bt_log(WARN, "sm", "dropped packet on SM channel!");
        }
      },
      [self]() {
        if (self) {
          self->OnChannelClosed();
        }
      },
      async_get_default_dispatcher());
  // The SMP fixed channel's MTU must be >=23 bytes (kNoSecureConnectionsMTU) per spec V5.0 Vol. 3
  // Part H 3.2. As SMP operates on a fixed channel, there is no way to configure this MTU, so we
  // expect that L2CAP always provides a channel with a sufficiently large MTU. This assertion
  // serves as an explicit acknowledgement of that contract between L2CAP and SMP.
  ZX_ASSERT(chan_->max_tx_sdu_size() >= kNoSecureConnectionsMtu &&
            chan_->max_rx_sdu_size() >= kNoSecureConnectionsMtu);
}

void PairingChannel::SetChannelHandler(fxl::WeakPtr<Handler> new_handler) {
  ZX_ASSERT(new_handler);
  bt_log(SPEW, "sm", "changing pairing channel handler");
  handler_ = std::move(new_handler);
}

void PairingChannel::OnRxBFrame(ByteBufferPtr sdu) {
  if (handler_) {
    handler_->OnRxBFrame(std::move(sdu));
  } else {
    bt_log(WARN, "sm", "no handler to receive L2CAP packet callback!");
  }
}

void PairingChannel::OnChannelClosed() {
  if (handler_) {
    handler_->OnChannelClosed();
  } else {
    bt_log(WARN, "sm", "no handler to receive L2CAP channel closed callback!");
  }
}

}  // namespace sm
}  // namespace bt
