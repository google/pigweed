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

#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

L2capReadChannel::L2capReadChannel(L2capReadChannel&& other)
    : connection_handle_(other.connection_handle()),
      local_cid_(other.local_cid()),
      payload_from_controller_fn_(std::move(other.payload_from_controller_fn_)),
      l2cap_channel_manager_(other.l2cap_channel_manager_) {
  l2cap_channel_manager_.ReleaseReadChannel(other);
  l2cap_channel_manager_.RegisterReadChannel(*this);
}

L2capReadChannel& L2capReadChannel::operator=(L2capReadChannel&& other) {
  if (this == &other) {
    return *this;
  }
  l2cap_channel_manager_.ReleaseReadChannel(*this);
  connection_handle_ = other.connection_handle();
  local_cid_ = other.local_cid();
  payload_from_controller_fn_ = std::move(other.payload_from_controller_fn_);
  l2cap_channel_manager_.ReleaseReadChannel(other);
  l2cap_channel_manager_.RegisterReadChannel(*this);
  return *this;
}

L2capReadChannel::~L2capReadChannel() {
  l2cap_channel_manager_.ReleaseReadChannel(*this);
}

void L2capReadChannel::OnFragmentedPduReceived() {
  PW_LOG_ERROR(
      "(CID 0x%X) Fragmented L2CAP frame received, which is not yet supported.",
      local_cid());
}

L2capReadChannel::L2capReadChannel(
    L2capChannelManager& l2cap_channel_manager,
    pw::Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    uint16_t connection_handle,
    uint16_t local_cid)
    : connection_handle_(connection_handle),
      local_cid_(local_cid),
      payload_from_controller_fn_(std::move(payload_from_controller_fn)),
      l2cap_channel_manager_(l2cap_channel_manager) {
  l2cap_channel_manager_.RegisterReadChannel(*this);
}

bool L2capReadChannel::AreValidParameters(uint16_t connection_handle,
                                          uint16_t local_cid) {
  if (connection_handle > kMaxValidConnectionHandle) {
    PW_LOG_ERROR(
        "Invalid connection handle 0x%X. Maximum connection handle is 0x0EFF.",
        connection_handle);
    return false;
  }
  if (local_cid == 0) {
    PW_LOG_ERROR("L2CAP channel identifier 0 is not valid.");
    return false;
  }
  return true;
}

}  // namespace pw::bluetooth::proxy
