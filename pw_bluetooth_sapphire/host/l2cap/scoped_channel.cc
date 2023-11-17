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

#include "pw_bluetooth_sapphire/internal/host/l2cap/scoped_channel.h"

namespace bt::l2cap {

ScopedChannel::ScopedChannel(Channel::WeakPtr chan) : chan_(std::move(chan)) {}

ScopedChannel::ScopedChannel(ScopedChannel&& other)
    : chan_(std::move(other.chan_)) {}

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
