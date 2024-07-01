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
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/emboss_util.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

ProxyHost::ProxyHost(
    pw::Function<void(H4PacketWithHci&& packet)>&& send_to_host_fn,
    pw::Function<void(H4PacketWithH4&& packet)>&& send_to_controller_fn,
    uint16_t le_acl_credits_to_reserve)
    : hci_transport_(std::move(send_to_host_fn),
                     std::move(send_to_controller_fn)),
      acl_data_channel_(hci_transport_, le_acl_credits_to_reserve),
      acl_send_pending_(false) {}

void ProxyHost::HandleH4HciFromHost(H4PacketWithH4&& h4_packet) {
  hci_transport_.SendToController(std::move(h4_packet));
}

void ProxyHost::ProcessH4HciFromController(pw::span<uint8_t> hci_buffer) {
  auto event = MakeEmboss<emboss::EventHeaderView>(hci_buffer);
  if (!event.IsComplete()) {
    PW_LOG_ERROR("Buffer is too small for EventHeader. So will not process.");
    return;
  }

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (event.event_code_enum().Read()) {
    case emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS: {
      auto nocp_event =
          MakeEmboss<emboss::NumberOfCompletedPacketsEventWriter>(hci_buffer);
      if (!nocp_event.IsComplete()) {
        PW_LOG_ERROR(
            "Buffer is too small for NUMBER_OF_COMPLETED_PACKETS event. So "
            "will not process.");
        break;
      }
      acl_data_channel_.ProcessNumberOfCompletedPacketsEvent(nocp_event);
      break;
    }
    case emboss::EventCode::DISCONNECTION_COMPLETE: {
      auto dc_event =
          MakeEmboss<emboss::DisconnectionCompleteEventWriter>(hci_buffer);
      if (!dc_event.IsComplete()) {
        PW_LOG_ERROR(
            "Buffer is too small for DISCONNECTION_COMPLETE event. So will not "
            "process.");
        break;
      }
      acl_data_channel_.ProcessDisconnectionCompleteEvent(dc_event);
      break;
    }
    case emboss::EventCode::COMMAND_COMPLETE: {
      ProcessCommandCompleteEvent(hci_buffer);
      break;
    }
    default: {
      PW_LOG_ERROR("Received unexpected HCI event. So will not process.");
      return;
    }
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

void ProxyHost::ProcessCommandCompleteEvent(pw::span<uint8_t> hci_buffer) {
  auto command_complete_event =
      MakeEmboss<emboss::CommandCompleteEventView>(hci_buffer);
  if (!command_complete_event.IsComplete()) {
    PW_LOG_ERROR(
        "Buffer is too small for COMMAND_COMPLETE event. So will not process.");
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
      // Nothing to process
      break;
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

void ProxyHost::HandleH4HciFromController(H4PacketWithHci&& h4_packet) {
  if (h4_packet.GetHciSpan().empty()) {
    PW_LOG_ERROR("Received empty H4 buffer. So will not process.");
  } else if (h4_packet.GetH4Type() == emboss::H4PacketType::EVENT) {
    ProcessH4HciFromController(h4_packet.GetHciSpan());
  }
  hci_transport_.SendToHost(std::move(h4_packet));
}

void ProxyHost::Reset() {
  acl_send_mutex_.lock();
  acl_data_channel_.Reset();
  acl_send_pending_ = false;
  acl_send_mutex_.unlock();
}

pw::Status ProxyHost::SendGattNotify(uint16_t connection_handle,
                                     uint16_t attribute_handle,
                                     pw::span<const uint8_t> attribute_value) {
  if (connection_handle > 0x0EFF) {
    PW_LOG_ERROR(
        "Invalid connection handle: %d (max: 0x0EFF). So will not process.",
        connection_handle);
    return pw::Status::InvalidArgument();
  }
  if (attribute_handle == 0) {
    PW_LOG_ERROR("Attribute handle cannot be 0. So will not process.");
    return pw::Status::InvalidArgument();
  }
  if (constexpr uint16_t kMaxAttributeSize =
          kH4BuffSize - 1 - emboss::AttNotifyOverAcl::MinSizeInBytes();
      attribute_value.size() > kMaxAttributeSize) {
    PW_LOG_ERROR("Attribute too large (%zu > %d). So will not process.",
                 attribute_value.size(),
                 kMaxAttributeSize);
    return pw::Status::InvalidArgument();
  }

  H4PacketWithH4 h4_att_notify({});
  {
    acl_send_mutex_.lock();
    // TODO: https://pwbug.dev/348680331 - Currently ProxyHost only supports 1
    // in-flight ACL send, increase this to support multiple.
    if (acl_send_pending_) {
      acl_send_mutex_.unlock();
      return pw::Status::Unavailable();
    }
    acl_send_pending_ = true;

    size_t acl_packet_size =
        emboss::AttNotifyOverAcl::MinSizeInBytes() + attribute_value.size();
    emboss::AttNotifyOverAclWriter att_notify =
        emboss::MakeAttNotifyOverAclView(attribute_value.size(),
                                         H4HciSubspan(h4_buff_).data(),
                                         acl_packet_size);
    if (!att_notify.IsComplete()) {
      PW_LOG_ERROR("Buffer is too small for ATT Notify. So will not send.");
      acl_send_mutex_.unlock();
      return pw::Status::InvalidArgument();
    }

    BuildAttNotify(
        att_notify, connection_handle, attribute_handle, attribute_value);

    size_t h4_packet_size = 1 + acl_packet_size;
    H4PacketWithH4 h4_temp(pw::span(h4_buff_.data(), h4_packet_size),
                           [this](const uint8_t* buffer) {
                             acl_send_mutex_.lock();
                             PW_CHECK_PTR_EQ(
                                 buffer,
                                 h4_buff_.data(),
                                 "Received release callback for buffer that "
                                 "doesn't match our buffer.");
                             PW_LOG_DEBUG("H4 packet release fn called.");
                             acl_send_pending_ = false;
                             acl_send_mutex_.unlock();
                           });
    h4_temp.SetH4Type(emboss::H4PacketType::ACL_DATA);
    h4_att_notify = std::move(h4_temp);
    acl_send_mutex_.unlock();
  }

  // H4 packet is hereby moved. Either ACL data channel will move packet to
  // controller or will be unable to send packet. In either case, packet will be
  // destructed, so its release function will clear the `acl_send_pending` flag.
  if (!acl_data_channel_.SendAcl(std::move(h4_att_notify))) {
    return pw::Status::Unavailable();
  }
  return OkStatus();
}

void ProxyHost::BuildAttNotify(emboss::AttNotifyOverAclWriter att_notify,
                               uint16_t connection_handle,
                               uint16_t attribute_handle,
                               pw::span<const uint8_t> attribute_value) {
  // ACL header
  att_notify.acl_header().handle().Write(connection_handle);
  att_notify.acl_header().packet_boundary_flag().Write(
      emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
  att_notify.acl_header().broadcast_flag().Write(
      emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
  size_t att_pdu_size =
      emboss::AttHandleValueNtf::MinSizeInBytes() + attribute_value.size();
  att_notify.acl_header().data_total_length().Write(
      emboss::BFrameHeader::IntrinsicSizeInBytes() + att_pdu_size);

  // L2CAP header
  // TODO: https://pwbug.dev/349602172 - Define ATT CID in pw_bluetooth.
  constexpr uint16_t kAttributeProtocolCID = 0x0004;
  att_notify.l2cap_header().channel_id().Write(kAttributeProtocolCID);
  att_notify.l2cap_header().pdu_length().Write(att_pdu_size);

  // ATT PDU
  att_notify.att_handle_value_ntf().attribute_opcode().Write(
      emboss::AttOpcode::ATT_HANDLE_VALUE_NTF);
  att_notify.att_handle_value_ntf().attribute_handle().Write(attribute_handle);
  std::memcpy(att_notify.att_handle_value_ntf()
                  .attribute_value()
                  .BackingStorage()
                  .data(),
              attribute_value.data(),
              attribute_value.size());
}

bool ProxyHost::HasSendAclCapability() const {
  return acl_data_channel_.GetLeAclCreditsToReserve() > 0;
}

uint16_t ProxyHost::GetNumFreeLeAclPackets() const {
  return acl_data_channel_.GetNumFreeLeAclPackets();
}

}  // namespace pw::bluetooth::proxy
