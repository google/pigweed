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

#include "pw_assert/check.h"  // IWYU pragma: keep
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/gatt_notify_channel_internal.h"
#include "pw_bluetooth_proxy/internal/l2cap_coc_internal.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

ProxyHost::ProxyHost(
    pw::Function<void(H4PacketWithHci&& packet)>&& send_to_host_fn,
    pw::Function<void(H4PacketWithH4&& packet)>&& send_to_controller_fn,
    uint16_t le_acl_credits_to_reserve)
    : hci_transport_(std::move(send_to_host_fn),
                     std::move(send_to_controller_fn)),
      acl_data_channel_(hci_transport_, le_acl_credits_to_reserve) {}

void ProxyHost::HandleH4HciFromHost(H4PacketWithH4&& h4_packet) {
  hci_transport_.SendToController(std::move(h4_packet));
}

void ProxyHost::HandleH4HciFromController(H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  auto event = MakeEmboss<emboss::EventHeaderView>(hci_buffer);
  if (!event.IsComplete()) {
    PW_LOG_ERROR(
        "Buffer is too small for EventHeader. So will pass on to host without "
        "processing.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (event.event_code_enum().Read()) {
    case emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS: {
      acl_data_channel_.HandleNumberOfCompletedPacketsEvent(
          std::move(h4_packet));
      break;
    }
    case emboss::EventCode::DISCONNECTION_COMPLETE: {
      acl_data_channel_.HandleDisconnectionCompleteEvent(std::move(h4_packet));
      break;
    }
    case emboss::EventCode::COMMAND_COMPLETE: {
      HandleCommandCompleteEvent(std::move(h4_packet));
      break;
    }
    default: {
      hci_transport_.SendToHost(std::move(h4_packet));
      return;
    }
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

void ProxyHost::HandleCommandCompleteEvent(H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  emboss::CommandCompleteEventView command_complete_event =
      MakeEmboss<emboss::CommandCompleteEventView>(hci_buffer);
  if (!command_complete_event.IsComplete()) {
    PW_LOG_ERROR(
        "Buffer is too small for COMMAND_COMPLETE event. So will not process.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
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
      break;
  }
  PW_MODIFY_DIAGNOSTICS_POP();
  hci_transport_.SendToHost(std::move(h4_packet));
}

void ProxyHost::Reset() {
  acl_data_channel_.Reset();
  h4_storage_.Reset();
}

pw::Result<L2capCoc> ProxyHost::AcquireL2capCoc(
    uint16_t connection_handle,
    L2capCoc::CocConfig rx_config,
    L2capCoc::CocConfig tx_config,
    [[maybe_unused]] pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn,
    pw::Function<void(L2capCoc::Event event)>&& event_fn) {
  return L2capCocInternal::Create(acl_data_channel_,
                                  h4_storage_,
                                  connection_handle,
                                  rx_config,
                                  tx_config,
                                  std::move(event_fn));
}

pw::Status ProxyHost::SendGattNotify(uint16_t connection_handle,
                                     uint16_t attribute_handle,
                                     pw::span<const uint8_t> attribute_value) {
  // TODO: https://pwbug.dev/369709521 - Migrate clients to channel API.
  pw::Result<GattNotifyChannel> channel_result =
      GattNotifyChannelInternal::Create(
          acl_data_channel_, h4_storage_, connection_handle, attribute_handle);
  if (!channel_result.ok()) {
    return channel_result.status();
  }
  return channel_result->Write(attribute_value);
}

bool ProxyHost::HasSendAclCapability() const {
  return acl_data_channel_.GetLeAclCreditsToReserve() > 0;
}

uint16_t ProxyHost::GetNumFreeLeAclPackets() const {
  return acl_data_channel_.GetNumFreeLeAclPackets();
}

}  // namespace pw::bluetooth::proxy
