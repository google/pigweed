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

#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/basic_l2cap_channel.h"

namespace pw::bluetooth::proxy {

// Interface for L2CAP signaling channels, which can be either ACL-U signaling
// channels or LE-U signaling channels.
class L2capSignalingChannel : public BasicL2capChannel {
 public:
  explicit L2capSignalingChannel(L2capChannelManager& l2cap_channel_manager,
                                 uint16_t connection_handle,
                                 uint16_t local_cid);

  L2capSignalingChannel& operator=(L2capSignalingChannel&& other);

  // Process an individual signaling command.
  //
  // Returns false if the command is not processed, either because it is not
  // directed to a channel managed by `L2capChannelManager` or because we do
  // not listen for that type of command.
  bool HandleL2capSignalingCommand(emboss::L2capSignalingCommandView cmd);

  // Handle L2CAP_FLOW_CONTROL_CREDIT_IND.
  //
  // Returns false if the packet is invalid or not directed to a channel managed
  // by `L2capChannelManager`.
  bool HandleFlowControlCreditInd(emboss::L2capFlowControlCreditIndView cmd);

 protected:
  L2capChannelManager& l2cap_channel_manager_;
};

}  // namespace pw::bluetooth::proxy
