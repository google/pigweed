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

namespace pw::bluetooth::proxy {

/// `GattNotifyChannel` supports sending GATT characteristic notifications to a
/// remote peer.
class GattNotifyChannel : public L2capChannel {
 public:
  // @deprecated
  // TODO: https://pwbug.dev/379337272 - Delete this once all downstreams
  // have transitioned to Write(MultiBuf) for this channel type.
  Status Write(pw::span<const uint8_t> payload) override;

  // Also allow Write(MultiBuf) during transition from Write(span).
  // TODO: https://pwbug.dev/379337272 - Can delete once Write(span) above is
  // deleted.
  StatusWithMultiBuf Write(multibuf::MultiBuf&& payload) override {
    return WriteMultiBufAsSpan(std::move(payload));
  }

 protected:
  static pw::Result<GattNotifyChannel> Create(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      uint16_t attribute_handle);

  bool DoHandlePduFromController(pw::span<uint8_t>) override {
    // Forward all packets to host.
    return false;
  }

  bool HandlePduFromHost(pw::span<uint8_t>) override {
    // Forward all packets to controller.
    return false;
  }

 private:
  // TODO: https://pwbug.dev/349602172 - Define ATT CID in pw_bluetooth.
  static constexpr uint16_t kAttributeProtocolCID = 0x0004;

  explicit GattNotifyChannel(L2capChannelManager& l2cap_channel_manager,
                             uint16_t connection_handle,
                             uint16_t attribute_handle);

  uint16_t attribute_handle_;
};

}  // namespace pw::bluetooth::proxy
