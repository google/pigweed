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

#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"

namespace pw::bluetooth::proxy {

// Signaling channel for managing L2CAP channels over LE-U logical links.
class L2capLeUSignalingChannel : public L2capSignalingChannel {
 public:
  explicit L2capLeUSignalingChannel(L2capChannelManager& l2cap_channel_manager,
                                    uint16_t connection_handle);

 private:
  // Fixed channel ID for LE-U signaling channels.
  // TODO: https://pwbug.dev/360929142 - Move this constant to Emboss.
  static constexpr uint16_t kLeUSignalingCid = 0x0005;

  // Process a C-frame, which must only contain one signaling command on LE-U
  // signaling channels.
  //
  // Returns false if the C-frame is to be forwarded on to the Bluetooth host,
  // either because the command is not directed towards a channel managed by
  // `L2capChannelManager` or because the C-frame is invalid and should be
  // handled by the Bluetooth host.
  bool OnPduReceived(pw::span<uint8_t> cframe) override;

  void OnFragmentedPduReceived() override;
};

}  // namespace pw::bluetooth::proxy
