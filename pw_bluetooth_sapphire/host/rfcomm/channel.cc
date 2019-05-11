// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel.h"

#include <lib/async/cpp/task.h>

#include "session.h"

namespace bt {
namespace rfcomm {

Channel::Channel(DLCI dlci, Session* session)
    : dlci_(dlci),
      session_(session),
      established_(false),
      negotiation_state_(ParameterNegotiationState::kNotNegotiated),
      local_credits_(0),
      remote_credits_(0),
      wait_queue_{} {}

size_t Channel::tx_mtu() const { return session_->GetMaximumUserDataLength(); }

namespace internal {

ChannelImpl::ChannelImpl(DLCI dlci, Session* session)
    : Channel(dlci, session) {}

bool ChannelImpl::Activate(RxCallback rx_callback,
                           ClosedCallback closed_callback,
                           async_dispatcher_t* dispatcher) {
  rx_callback_ = std::move(rx_callback);
  closed_callback_ = std::move(closed_callback_);
  dispatcher_ = dispatcher;

  while (!pending_rxed_frames_.empty()) {
    auto& data = pending_rxed_frames_.front();
    async::PostTask(dispatcher_, [this, data_ = std::move(data)]() mutable {
      rx_callback_(std::move(data_));
    });
    pending_rxed_frames_.pop();
  }

  return true;
}

bool ChannelImpl::Send(ByteBufferPtr data) {
  ZX_DEBUG_ASSERT(session_);
  ZX_DEBUG_ASSERT_MSG(rx_callback_, "must call Activate() first");
  session_->SendUserData(dlci_, std::move(data));
  return true;
}

void ChannelImpl::Receive(ByteBufferPtr data) {
  if (rx_callback_) {
    async::PostTask(dispatcher_, [this, data_ = std::move(data)]() mutable {
      rx_callback_(std::move(data_));
    });
  } else {
    pending_rxed_frames_.push(std::move(data));
  }
}

}  // namespace internal
}  // namespace rfcomm
}  // namespace bt
