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

#include "pw_bluetooth_proxy/internal/l2cap_aclu_signaling_channel.h"

#include "pw_assert/check.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/direction.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

L2capAclUSignalingChannel::L2capAclUSignalingChannel(
    L2capChannelManager& l2cap_channel_manager, uint16_t connection_handle)
    : L2capSignalingChannel(
          /*l2cap_channel_manager=*/l2cap_channel_manager,
          /*connection_handle=*/connection_handle,
          /*transport=*/AclTransportType::kBrEdr,
          /*fixed_cid=*/
          cpp23::to_underlying(emboss::L2capFixedCid::ACL_U_SIGNALING)) {}

bool L2capAclUSignalingChannel::OnCFramePayload(
    Direction direction, pw::span<const uint8_t> cframe_payload) {
  bool all_consumed = false;

  do {
    auto cmd = emboss::MakeL2capSignalingCommandView(cframe_payload.data(),
                                                     cframe_payload.size());
    if (!cmd.Ok()) {
      PW_LOG_ERROR(
          "Remaining buffer is too small for L2CAP command. So will forward "
          "without processing.");

      // TODO: https://pwbug.dev/379172336 - Handle partially consumed ACL-U
      // signaling command packets.
      PW_CHECK(!all_consumed, "Consumed some commands.");
      return false;
    }
    bool current_consumed = HandleL2capSignalingCommand(direction, cmd);

    // TODO: https://pwbug.dev/379172336 - Handle partially consumed ACL-U
    // signaling command packets.
    PW_CHECK(all_consumed == current_consumed,
             "Wasn't able to consume all commands, but don't yet support "
             "passing on some of them");
    all_consumed |= current_consumed;
    cframe_payload = cframe_payload.subspan(cmd.SizeInBytes());
  } while (!cframe_payload.empty());

  return all_consumed;
}

}  // namespace pw::bluetooth::proxy
