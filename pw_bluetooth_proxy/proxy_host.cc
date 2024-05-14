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

#include "pw_bluetooth_proxy/proxy_host.h"

#include "emboss_util.h"
#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/common.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

ProxyHost::ProxyHost(H4HciPacketSendFn&& send_to_host_fn,
                     H4HciPacketSendFn&& send_to_controller_fn,
                     uint16_t le_acl_credits_to_reserve)
    : outward_send_to_host_fn_(std::move(send_to_host_fn)),
      outward_send_to_controller_fn_(std::move(send_to_controller_fn)),
      acl_data_channel_{le_acl_credits_to_reserve} {}

void ProxyHost::HandleH4HciFromHost(H4HciPacket h4_packet) {
  SendToController(h4_packet);
}

void ProxyHost::ProcessH4HciFromController(H4HciPacket h4_packet) {
  if (h4_packet.empty()) {
    PW_LOG_ERROR("Received empty H4 buffer. So will not process.");
    return;
  }

  if (h4_packet[0] != cpp23::to_underlying(emboss::H4PacketType::EVENT)) {
    return;
  }
  pw::span hci_buffer = H4HciSubspan(h4_packet);
  auto event = MakeEmboss<emboss::EventHeaderView>(hci_buffer);
  if (!event.IsComplete()) {
    PW_LOG_ERROR("Buffer is too small for EventHeader. So will not process.");
    return;
  }

  if (event.event_code_enum().Read() != emboss::EventCode::COMMAND_COMPLETE) {
    return;
  }
  auto command_complete_event =
      MakeEmboss<emboss::CommandCompleteEventView>(hci_buffer);
  if (!command_complete_event.IsComplete()) {
    PW_LOG_ERROR(
        "Buffer is too small for COMMAND_COMPLETE event. So will not process.");
    return;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
  switch (command_complete_event.command_opcode_enum().Read()) {
    case emboss::OpCode::LE_READ_BUFFER_SIZE_V1: {
      auto read_event =
          MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
              hci_buffer);
      if (!read_event.IsComplete()) {
        PW_LOG_ERROR(
            "Buffer is too small for LE_READ_BUFFER_SIZE_V1 command complete "
            "event. So will not process.");
        return;
      }
      acl_data_channel_.ProcessLEReadBufferSizeCommandCompleteEvent(read_event);
      break;
    }
    case emboss::OpCode::LE_READ_BUFFER_SIZE_V2: {
      auto read_event =
          MakeEmboss<emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
              hci_buffer);
      if (!read_event.IsComplete()) {
        PW_LOG_ERROR(
            "Buffer is too small for LE_READ_BUFFER_SIZE_V2 command complete "
            "event. So will not process.");
        return;
      }
      acl_data_channel_.ProcessLEReadBufferSizeCommandCompleteEvent(read_event);
      break;
    }
    default:
      // Nothing to process
      break;
  }
#pragma clang diagnostic pop
}

void ProxyHost::HandleH4HciFromController(H4HciPacket h4_packet) {
  ProcessH4HciFromController(h4_packet);
  SendToHost(h4_packet);
}

void ProxyHost::SendToHost(H4HciPacket h4_packet) {
  PW_DCHECK(outward_send_to_host_fn_ != nullptr);
  outward_send_to_host_fn_(h4_packet);
}

void ProxyHost::SendToController(H4HciPacket h4_packet) {
  PW_DCHECK(outward_send_to_controller_fn_ != nullptr);
  outward_send_to_controller_fn_(h4_packet);
}

uint16_t ProxyHost::GetNumFreeLeAclPackets() const {
  return acl_data_channel_.GetNumFreeLeAclPackets();
}

}  // namespace pw::bluetooth::proxy
