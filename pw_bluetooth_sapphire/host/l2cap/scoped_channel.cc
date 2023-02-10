// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_channel.h"

namespace bt::l2cap {

ScopedChannel::ScopedChannel(Channel::WeakPtr chan) : chan_(std::move(chan)) {}

ScopedChannel::ScopedChannel(ScopedChannel&& other) : chan_(std::move(other.chan_)) {}

ScopedChannel::~ScopedChannel() { Close(); }

void ScopedChannel::Reset(Channel::WeakPtr new_channel) {
  if (chan_.is_alive()) {
    Close();
  }
  chan_ = std::move(new_channel);
}

void ScopedChannel::Close() {
  if (chan_.is_alive()) {
    chan_->Deactivate();
    chan_ = Channel::WeakPtr();
  }
}

}  // namespace bt::l2cap
