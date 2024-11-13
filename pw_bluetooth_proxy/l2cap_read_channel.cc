// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_proxy/internal/l2cap_read_channel.h"

namespace pw::bluetooth::proxy {

L2capReadChannel::L2capReadChannel(L2capReadChannel&& other)
    : connection_handle_(other.connection_handle()),
      local_cid_(other.local_cid()),
      read_channels_(other.read_channels_),
      receive_fn_(std::move(other.receive_fn_)) {
  read_channels_.remove(other);
  read_channels_.push_front(*this);
}

L2capReadChannel::~L2capReadChannel() { read_channels_.remove(*this); }

L2capReadChannel::L2capReadChannel(
    uint16_t connection_handle,
    uint16_t local_cid,
    IntrusiveForwardList<L2capReadChannel>& read_channels,
    pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn)
    : connection_handle_(connection_handle),
      local_cid_(local_cid),
      read_channels_(read_channels),
      receive_fn_(std::move(receive_fn)) {
  read_channels_.push_front(*this);
}

}  // namespace pw::bluetooth::proxy
