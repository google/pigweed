// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_channel.h"

namespace btlib {
namespace l2cap {

ScopedChannel::ScopedChannel(fbl::RefPtr<Channel> chan) : chan_(chan) {}

ScopedChannel::~ScopedChannel() { Close(); }

void ScopedChannel::Reset(fbl::RefPtr<Channel> new_channel) {
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

}  // namespace l2cap
}  // namespace btlib
