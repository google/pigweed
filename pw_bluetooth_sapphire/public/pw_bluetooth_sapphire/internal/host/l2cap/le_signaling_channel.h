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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/le_connection_parameters.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/signaling_channel.h"

namespace bt::l2cap::internal {

// Implements the L2CAP LE signaling fixed channel.
class LESignalingChannel final : public SignalingChannel {
 public:
  LESignalingChannel(Channel::WeakPtr chan,
                     pw::bluetooth::emboss::ConnectionRole role,
                     pw::async::Dispatcher& dispatcher);
  ~LESignalingChannel() override = default;

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu,
                    const SignalingPacketHandler& cb) override;
  bool IsSupportedResponse(CommandCode code) const override;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LESignalingChannel);
};

}  // namespace bt::l2cap::internal
