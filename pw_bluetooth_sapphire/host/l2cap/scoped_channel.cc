// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_channel.h"

namespace bt::l2cap {

ScopedChannel::ScopedChannel(fxl::WeakPtr<Channel> chan) : chan_(std::move(chan)) {}

ScopedChannel::~ScopedChannel() { Close(); }

void ScopedChannel::Reset(fxl::WeakPtr<Channel> new_channel) {
  if (chan_) {
    Close();
  }
  chan_ = std::move(new_channel);
}

void ScopedChannel::Close() {
  if (chan_) {
    chan_->Deactivate();
    chan_ = nullptr;
  }
}

}  // namespace bt::l2cap
