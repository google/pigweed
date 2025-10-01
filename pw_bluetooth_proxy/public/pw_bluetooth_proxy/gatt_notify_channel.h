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

#include <cstdint>

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/single_channel_proxy.h"

namespace pw::bluetooth::proxy {

/// `GattNotifyChannel` supports sending GATT characteristic notifications to a
/// remote peer.
// TODO: https://pwbug.dev/388082771 - Switch to only inheriting from
// SingleChannelProxy (which will hold on to underlying L2capChannel).
class GattNotifyChannel : public SingleChannelProxy {
 public:
  GattNotifyChannel(const GattNotifyChannel& other) = delete;
  GattNotifyChannel& operator=(const GattNotifyChannel& other) = delete;
  GattNotifyChannel(GattNotifyChannel&&) = default;
  GattNotifyChannel& operator=(GattNotifyChannel&& other) = default;
  ~GattNotifyChannel() override;

  /// Return the attribute handle of this GattNotify channel.
  uint16_t attribute_handle() const { return attribute_handle_; }

  /// Check if the passed Write parameter is acceptable.
  Status DoCheckWriteParameter(pw::multibuf::MultiBuf& payload) override;

 protected:
  static pw::Result<GattNotifyChannel> Create(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      uint16_t attribute_handle,
      ChannelEventCallback&& event_fn);

  bool DoHandlePduFromController(pw::span<uint8_t>) override {
    // Forward all packets to host.
    return false;
  }

  bool HandlePduFromHost(pw::span<uint8_t>) override {
    // Forward all packets to controller.
    return false;
  }

  void DoClose() override {}

 private:
  [[nodiscard]] std::optional<H4PacketWithH4> GenerateNextTxPacket()
      PW_EXCLUSIVE_LOCKS_REQUIRED(l2cap_tx_mutex()) override;

  explicit GattNotifyChannel(L2capChannelManager& l2cap_channel_manager,
                             uint16_t connection_handle,
                             uint16_t attribute_handle,
                             ChannelEventCallback&& event_fn);

  uint16_t attribute_handle_;
};

}  // namespace pw::bluetooth::proxy
