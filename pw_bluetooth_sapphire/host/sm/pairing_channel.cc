// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_channel.h"

#include "lib/async/default.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sm {

PairingChannel::PairingChannel(l2cap::Channel::WeakPtr chan, fit::closure timer_resetter)
    : chan_(std::move(chan)), reset_timer_(std::move(timer_resetter)), weak_ptr_factory_(this) {
  BT_ASSERT(chan_);
  BT_ASSERT(async_get_default_dispatcher());
  if (chan_->link_type() == bt::LinkType::kLE) {
    BT_ASSERT(chan_->id() == l2cap::kLESMPChannelId);
  } else if (chan_->link_type() == bt::LinkType::kACL) {
    BT_ASSERT(chan_->id() == l2cap::kSMPChannelId);
  } else {
    BT_PANIC("unsupported link type for SMP!");
  }
  auto self = weak_ptr_factory_.GetWeakPtr();
  chan_->Activate(
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
      });
  // The SMP fixed channel's MTU must be >=23 bytes (kNoSecureConnectionsMTU) per spec V5.0 Vol. 3
  // Part H 3.2. As SMP operates on a fixed channel, there is no way to configure this MTU, so we
  // expect that L2CAP always provides a channel with a sufficiently large MTU. This assertion
  // serves as an explicit acknowledgement of that contract between L2CAP and SMP.
  BT_ASSERT(chan_->max_tx_sdu_size() >= kNoSecureConnectionsMtu &&
            chan_->max_rx_sdu_size() >= kNoSecureConnectionsMtu);
}

PairingChannel::PairingChannel(l2cap::Channel::WeakPtr chan)
    : PairingChannel(std::move(chan), []() {}) {}

void PairingChannel::SetChannelHandler(fxl::WeakPtr<Handler> new_handler) {
  BT_ASSERT(new_handler);
  bt_log(TRACE, "sm", "changing pairing channel handler");
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

}  // namespace bt::sm
