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

#pragma once

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"

namespace pw::bluetooth::proxy {

class BasicL2capChannel : public L2capChannel {
 public:
  // TODO: https://pwbug.dev/360929142 - Take the MTU. Signaling channels would
  // provide MTU_SIG.
  static pw::Result<BasicL2capChannel> Create(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      Function<bool(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  BasicL2capChannel(const BasicL2capChannel& other) = delete;
  BasicL2capChannel& operator=(const BasicL2capChannel& other) = delete;
  BasicL2capChannel(BasicL2capChannel&&) = default;
  // Move assignment operator allows channels to be erased from pw_containers.
  BasicL2capChannel& operator=(BasicL2capChannel&& other) = default;
  ~BasicL2capChannel() override;

  // @deprecated
  // TODO: https://pwbug.dev/379337272 - Delete this once all downstreams
  // have transitioned to Write(MultiBuf) for this channel type.
  Status Write(pw::span<const uint8_t> payload) override;

  // Also make visible Write(MultiBuf)
  // TODO: https://pwbug.dev/379337272 - Can delete when Write(span) is deleted.
  using L2capChannel::Write;

 protected:
  explicit BasicL2capChannel(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      Function<bool(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  bool HandlePduFromHost(pw::span<uint8_t> bframe) override;

 private:
  bool DoHandlePduFromController(pw::span<uint8_t> bframe) override;

  // TODO: https://pwbug.dev/379337272 - Move to true once this channel uses
  // payload queue. Delete once all downstreams have transitioned to
  // Write(MultiBuf) for this channel type.
  bool UsesPayloadQueue() override { return false; }
};

}  // namespace pw::bluetooth::proxy
