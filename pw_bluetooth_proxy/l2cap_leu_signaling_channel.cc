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
          /*local_cid=*/kLeUSignalingCid) {}

bool L2capLeUSignalingChannel::OnPduReceived(pw::span<uint8_t> cframe) {
  Result<emboss::CFrameView> cframe_view =
      MakeEmbossView<emboss::CFrameView>(cframe);
  if (!cframe_view.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for C-frame. So will forward to host without "
        "processing.");
    return false;
  }

  emboss::L2capSignalingCommandHeaderView cmd_header =
      emboss::MakeL2capSignalingCommandHeaderView(
          cframe_view->payload().BackingStorage().data(),
          emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  if (!cmd_header.IsComplete()) {
    PW_LOG_ERROR(
        "C-frame is not long enough to contain a valid command. So will "
        "forward to host without processing.");
    return false;
  }

  // TODO: https://pwbug.dev/360929142 - "If a device receives a C-frame that
  // exceeds its L2CAP_SIG_MTU_SIZE then it shall send an
  // L2CAP_COMMAND_REJECT_RSP packet containing the supported
  // L2CAP_SIG_MTU_SIZE." We should consider taking the signaling MTU in the
  // ProxyHost constructor.

  // Core Spec v5.4 Vol 3, Part A, 4: "Examples of signaling packets that are
  // not correctly formed include... A C-frame on fixed channel 0x0005 contains
  // more than one signaling packet"
  size_t cmd_length =
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes() +
      cmd_header.data_length().Read();
  if (cframe_view->pdu_length().Read() > cmd_length) {
    PW_LOG_ERROR(
        "Received C-frame on LE-U signaling channel with payload larger than "
        "its command. So will forward to host without processing.");
    return false;
  }

  emboss::L2capSignalingCommandView view =
      emboss::MakeL2capSignalingCommandView(
          cframe_view->payload().BackingStorage().data(),
          cframe_view->payload().BackingStorage().SizeInBytes());
  if (!view.IsComplete()) {
    PW_LOG_ERROR(
        "L2CAP PDU payload length not enough to accommodate signaling command. "
        "So will forward to host without processing.");
    return false;
  }

  return HandleL2capSignalingCommand(view);
}

void L2capLeUSignalingChannel::OnFragmentedPduReceived() {
  PW_LOG_ERROR(
      "(Connection: 0x%X) Received fragmented L2CAP frame on LE-U signaling "
      "channel.",
      connection_handle());
}

}  // namespace pw::bluetooth::proxy
