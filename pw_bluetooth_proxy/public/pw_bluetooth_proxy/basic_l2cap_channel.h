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
      Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
      Function<void()>&& queue_space_available_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  BasicL2capChannel(const BasicL2capChannel& other) = delete;
  BasicL2capChannel& operator=(const BasicL2capChannel& other) = delete;
  BasicL2capChannel(BasicL2capChannel&&) = default;
  // Move assignment operator allows channels to be erased from pw_containers.
  BasicL2capChannel& operator=(BasicL2capChannel&& other) = default;

  /// Send an L2CAP payload to the remote peer.
  ///
  /// @param[in] payload The L2CAP payload to be sent. Payload will be copied
  ///                    before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK:                  If packet was successfully queued for send.
  ///  UNAVAILABLE:         If channel could not acquire the resources to queue
  ///                       the send at this time (transient error). If a
  ///                       `queue_space_available_fn` has been provided it will
  ///                       be called when there is queue space available again.
  ///  INVALID_ARGUMENT:    If payload is too large.
  ///  FAILED_PRECONDITION  If channel is not `State::kRunning`.
  /// @endrst
  pw::Status Write(pw::span<const uint8_t> payload);

 protected:
  explicit BasicL2capChannel(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
      Function<void()>&& queue_space_available_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

 protected:
  bool HandlePduFromController(pw::span<uint8_t> bframe) override;
  bool HandlePduFromHost(pw::span<uint8_t> bframe) override;

  // TODO: https://pwbug.dev/360929142 - Stop channel on errors.
};

}  // namespace pw::bluetooth::proxy
