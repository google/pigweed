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

#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"

#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_bluetooth_proxy/internal/l2cap_coc_internal.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

L2capSignalingChannel::L2capSignalingChannel(
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t local_cid)
    : BasicL2capChannel(/*l2cap_channel_manager=*/l2cap_channel_manager,
                        /*connection_handle=*/connection_handle,
                        /*local_cid=*/local_cid,
                        /*receive_fn=*/nullptr),
      l2cap_channel_manager_(l2cap_channel_manager) {}

L2capSignalingChannel& L2capSignalingChannel::operator=(
    L2capSignalingChannel&& other) {
  BasicL2capChannel::operator=(std::move(other));
  return *this;
}

bool L2capSignalingChannel::OnPduReceived(pw::span<uint8_t> cframe) {
  Result<emboss::CFrameView> cframe_view =
      MakeEmbossView<emboss::CFrameView>(cframe);
  if (!cframe_view.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for C-frame. So will forward to host without "
        "processing.");
    return false;
  }

  // TODO: https://pwbug.dev/360929142 - "If a device receives a C-frame that
  // exceeds its L2CAP_SIG_MTU_SIZE then it shall send an
  // L2CAP_COMMAND_REJECT_RSP packet containing the supported
  // L2CAP_SIG_MTU_SIZE." We should consider taking the signaling MTU in the
  // ProxyHost constructor.
  return OnCFramePayload(
      pw::span(cframe_view->payload().BackingStorage().data(),
               cframe_view->payload().BackingStorage().SizeInBytes()));
}

bool L2capSignalingChannel::HandleL2capSignalingCommand(
    emboss::L2capSignalingCommandView cmd) {
  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (cmd.command_header().code().Read()) {
    case emboss::L2capSignalingPacketCode::FLOW_CONTROL_CREDIT_IND: {
      return HandleFlowControlCreditInd(
          emboss::MakeL2capFlowControlCreditIndView(cmd.BackingStorage().data(),
                                                    cmd.SizeInBytes()));
    }
    default: {
      return false;
    }
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

bool L2capSignalingChannel::HandleFlowControlCreditInd(
    emboss::L2capFlowControlCreditIndView cmd) {
  if (!cmd.IsComplete()) {
    PW_LOG_ERROR(
        "Buffer is too small for L2CAP_FLOW_CONTROL_CREDIT_IND. So will "
        "forward to host without processing.");
    return false;
  }

  L2capWriteChannel* found_channel = l2cap_channel_manager_.FindWriteChannel(
      connection_handle(), cmd.cid().Read());
  if (found_channel) {
    // If this L2CAP_FLOW_CONTROL_CREDIT_IND is addressed to a channel managed
    // by the proxy, it must be an L2CAP connection-oriented channel.
    // TODO: https://pwbug.dev/360929142 - Validate type in case remote peer
    // sends indication addressed to wrong CID.
    L2capCocInternal* coc_ptr = static_cast<L2capCocInternal*>(found_channel);
    coc_ptr->AddCredits(cmd.credits().Read());
    return true;
  }

  return false;
}

void L2capSignalingChannel::OnFragmentedPduReceived() {
  PW_LOG_ERROR(
      "(Connection: 0x%X) Received fragmentary ACL data on signaling channel.",
      connection_handle());
}

}  // namespace pw::bluetooth::proxy
