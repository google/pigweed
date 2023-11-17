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

#include "pw_bluetooth_sapphire/internal/host/sm/pairing_channel.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/scoped_channel.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"

namespace bt::sm {

PairingChannel::PairingChannel(l2cap::Channel::WeakPtr chan,
                               fit::closure timer_resetter)
    : chan_(std::move(chan)),
      reset_timer_(std::move(timer_resetter)),
      weak_self_(this) {
  BT_ASSERT(chan_);
  if (chan_->link_type() == bt::LinkType::kLE) {
    BT_ASSERT(chan_->id() == l2cap::kLESMPChannelId);
  } else if (chan_->link_type() == bt::LinkType::kACL) {
    BT_ASSERT(chan_->id() == l2cap::kSMPChannelId);
  } else {
    BT_PANIC("unsupported link type for SMP!");
  }
  auto self = weak_self_.GetWeakPtr();
  chan_->Activate(
      [self](ByteBufferPtr sdu) {
        if (self.is_alive()) {
          self->OnRxBFrame(std::move(sdu));
        } else {
          bt_log(WARN, "sm", "dropped packet on SM channel!");
        }
      },
      [self]() {
        if (self.is_alive()) {
          self->OnChannelClosed();
        }
      });
  // The SMP fixed channel's MTU must be >=23 bytes (kNoSecureConnectionsMTU)
  // per spec V5.0 Vol. 3 Part H 3.2. As SMP operates on a fixed channel, there
  // is no way to configure this MTU, so we expect that L2CAP always provides a
  // channel with a sufficiently large MTU. This assertion serves as an explicit
  // acknowledgement of that contract between L2CAP and SMP.
  BT_ASSERT(chan_->max_tx_sdu_size() >= kNoSecureConnectionsMtu &&
            chan_->max_rx_sdu_size() >= kNoSecureConnectionsMtu);
}

PairingChannel::PairingChannel(l2cap::Channel::WeakPtr chan)
    : PairingChannel(std::move(chan), []() {}) {}

void PairingChannel::SetChannelHandler(Handler::WeakPtr new_handler) {
  BT_ASSERT(new_handler.is_alive());
  bt_log(TRACE, "sm", "changing pairing channel handler");
  handler_ = std::move(new_handler);
}

void PairingChannel::OnRxBFrame(ByteBufferPtr sdu) {
  if (handler_.is_alive()) {
    handler_->OnRxBFrame(std::move(sdu));
  } else {
    bt_log(WARN, "sm", "no handler to receive L2CAP packet callback!");
  }
}

void PairingChannel::OnChannelClosed() {
  if (handler_.is_alive()) {
    handler_->OnChannelClosed();
  } else {
    bt_log(WARN, "sm", "no handler to receive L2CAP channel closed callback!");
  }
}

}  // namespace bt::sm
