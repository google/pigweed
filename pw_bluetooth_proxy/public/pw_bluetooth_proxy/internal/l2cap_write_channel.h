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

#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

class L2capChannelManager;

// Base class for peer-to-peer L2CAP-based channels supporting writing.
//
// Derived channels should expose, at minimum, a "write" operation that
// takes a variable information payload. Protocol-dependent information that
// is fixed per channel, such as addresses, flags, handles, etc. should be
// provided at construction to derived channels.
class L2capWriteChannel {
 public:
  virtual ~L2capWriteChannel() {}

 protected:
  explicit L2capWriteChannel(L2capChannelManager& l2cap_channel_manager,
                             uint16_t connection_handle,
                             uint16_t remote_cid);

  // Returns whether or not ACL connection handle & destination L2CAP channel
  // identifier are valid parameters for a packet.
  [[nodiscard]] static bool AreValidParameters(uint16_t connection_handle,
                                               uint16_t remote_cid);

  // Reserve an L2CAP over ACL over H4 packet, with those three headers
  // populated for an L2CAP PDU payload of `data_length` bytes addressed to
  // `connection_handle_`.
  //
  // Returns PW_STATUS_INVALID_ARGUMENT if payload is too large for a buffer.
  // Returns PW_STATUS_UNAVAILABLE if all buffers are currently occupied.
  pw::Result<H4PacketWithH4> PopulateTxL2capPacket(uint16_t data_length);

  // Send `tx_packet` over `AclDataChannel`.
  //
  // Returns PW_STATUS_OK if send successful.
  // Returns PW_STATUS_UNAVAILABLE if credits were not available to send.
  pw::Status SendL2capPacket(H4PacketWithH4&& tx_packet);

  // Returns the maximum size supported for Tx L2CAP PDU payloads.
  uint16_t MaxL2capPayloadSize() const;

 private:
  static constexpr uint16_t kMaxValidConnectionHandle = 0x0EFF;

  // ACL connection handle on remote peer to which packets are sent.
  uint16_t connection_handle_;

  // L2CAP channel ID on remote peer to which packets are sent.
  uint16_t remote_cid_;

  L2capChannelManager& l2cap_channel_manager_;
  // TODO: https://pwbug.dev/360934030 - Add Tx packet queue here.
};

}  // namespace pw::bluetooth::proxy
