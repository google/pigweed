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

#include "pw_bluetooth_proxy/internal/l2cap_leu_signaling_channel.h"

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

L2capLeUSignalingChannel::L2capLeUSignalingChannel(
    L2capChannelManager& l2cap_channel_manager, uint16_t connection_handle)
    : L2capSignalingChannel(
          /*l2cap_channel_manager=*/l2cap_channel_manager,
          /*connection_handle=*/connection_handle,
          /*local_cid=*/
          cpp23::to_underlying(emboss::L2capFixedCid::LE_U_SIGNALING)) {}

bool L2capLeUSignalingChannel::OnCFramePayload(
    pw::span<const uint8_t> cframe_payload) {
  emboss::L2capSignalingCommandHeaderView cmd_header =
      emboss::MakeL2capSignalingCommandHeaderView(cframe_payload.data(),
                                                  cframe_payload.size());
  if (!cmd_header.Ok()) {
    PW_LOG_ERROR(
        "C-frame does not contain a valid command. So will forward to host "
        "without processing.");
    return false;
  }

  // Core Spec v5.4 Vol 3, Part A, 4: "Examples of signaling packets that are
  // not correctly formed include... A C-frame on fixed channel 0x0005 contains
  // more than one signaling packet"
  const size_t cmd_length =
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes() +
      cmd_header.data_length().Read();
  if (cframe_payload.size() > cmd_length) {
    PW_LOG_ERROR(
        "Received C-frame on LE-U signaling channel with payload larger than "
        "its command. So will forward to host without processing.");
    return false;
  }

  emboss::L2capSignalingCommandView cmd = emboss::MakeL2capSignalingCommandView(
      cframe_payload.data(), cframe_payload.size());

  if (!cmd.Ok()) {
    PW_LOG_ERROR(
        "L2CAP PDU payload length not enough to accommodate signaling command. "
        "So will forward to host without processing.");
    return false;
  }

  return HandleL2capSignalingCommand(cmd);
}

}  // namespace pw::bluetooth::proxy
