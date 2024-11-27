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

#include "pw_containers/intrusive_forward_list.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace pw::bluetooth::proxy {

class L2capChannelManager;

// Base class for peer-to-peer L2CAP-based channels supporting reading.
//
// Read channels invoke a client-supplied read callback for packets sent by
// the peer to the channel.
class L2capReadChannel : public IntrusiveForwardList<L2capReadChannel>::Item {
 public:
  L2capReadChannel(const L2capReadChannel& other) = delete;
  L2capReadChannel& operator=(const L2capReadChannel& other) = delete;
  L2capReadChannel(L2capReadChannel&& other);
  // Move assignment operator allows channels to be erased from pw::Vector.
  L2capReadChannel& operator=(L2capReadChannel&& other);

  virtual ~L2capReadChannel();

  // Handle an Rx L2CAP PDU.
  //
  // Implementations should call `CallControllerReceiveFn` after
  // recombining/processing the PDU (e.g. after updating channel state and
  // screening out certain PDUs).
  //
  // Return true if the PDU was consumed by the channel. Otherwise, return false
  // and the PDU will be forwarded by `ProxyHost` on to the Bluetooth host.
  [[nodiscard]] virtual bool HandlePduFromController(
      pw::span<uint8_t> l2cap_pdu) = 0;

  // Handle a Tx L2CAP PDU.
  //
  // Implementations should call `CallHostReceiveFn` after
  // recombining/processing the PDU (e.g. after updating channel state and
  // screening out certain PDUs).
  //
  // Return true if the PDU was consumed by the channel. Otherwise, return false
  // and the PDU will be forwarded by `ProxyHost` on to the Bluetooth
  // controller.
  [[nodiscard]] virtual bool HandlePduFromHost(pw::span<uint8_t> l2cap_pdu) = 0;

  // Handle fragmented Rx L2CAP PDU.
  // TODO: https://pwbug.dev/365179076 - Support recombination & delete this.
  virtual void OnFragmentedPduReceived();

  // Get the source L2CAP channel ID.
  uint16_t local_cid() const { return local_cid_; }

  // Get the ACL connection handle.
  uint16_t connection_handle() const { return connection_handle_; }

 protected:
  explicit L2capReadChannel(
      L2capChannelManager& l2cap_channel_manager,
      pw::Function<void(pw::span<uint8_t> payload)>&& controller_receive_fn,
      uint16_t connection_handle,
      uint16_t local_cid);

  // Returns whether or not ACL connection handle & local L2CAP channel
  // identifier are valid parameters for a packet.
  [[nodiscard]] static bool AreValidParameters(uint16_t connection_handle,
                                               uint16_t local_cid);

  // Often the useful `payload` for clients is some subspan of the Rx SDU.
  void CallControllerReceiveFn(pw::span<uint8_t> payload) {
    if (controller_receive_fn_) {
      controller_receive_fn_(payload);
    }
  }

 private:
  static constexpr uint16_t kMaxValidConnectionHandle = 0x0EFF;

  // ACL connection handle of this channel.
  uint16_t connection_handle_;
  // L2CAP channel ID of this channel.
  uint16_t local_cid_;
  // Client-provided controller read callback.
  pw::Function<void(pw::span<uint8_t> payload)> controller_receive_fn_;

  L2capChannelManager& l2cap_channel_manager_;
};

}  // namespace pw::bluetooth::proxy
