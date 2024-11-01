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
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/emboss_util.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

std::array<containers::Pair<uint8_t*, bool>, ProxyHost::kNumH4Buffs>
ProxyHost::InitOccupiedMap() {
  std::array<containers::Pair<uint8_t*, bool>, kNumH4Buffs> arr;
  acl_send_mutex_.lock();
  for (size_t i = 0; i < kNumH4Buffs; ++i) {
    arr[i] = {h4_buffs_[i].data(), false};
  }
  acl_send_mutex_.unlock();
  return arr;
}

ProxyHost::ProxyHost(
    pw::Function<void(H4PacketWithHci&& packet)>&& send_to_host_fn,
    pw::Function<void(H4PacketWithH4&& packet)>&& send_to_controller_fn,
    uint16_t le_acl_credits_to_reserve)
    : hci_transport_(std::move(send_to_host_fn),
                     std::move(send_to_controller_fn)),
      acl_data_channel_(hci_transport_, le_acl_credits_to_reserve),
      h4_buff_occupied_(InitOccupiedMap()) {}

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
  acl_send_mutex_.lock();
  acl_data_channel_.Reset();
  for (const auto& [buff, _] : h4_buff_occupied_) {
    h4_buff_occupied_.at(buff) = false;
  }
  acl_send_mutex_.unlock();
}

std::optional<pw::span<uint8_t>> ProxyHost::ReserveH4Buff() {
  for (const auto& [buff, occupied] : h4_buff_occupied_) {
    if (!occupied) {
      h4_buff_occupied_.at(buff) = true;
      return {{buff, kH4BuffSize}};
    }
  }
  return std::nullopt;
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

  H4PacketWithH4 h4_att_notify;
  {
    acl_send_mutex_.lock();
    std::optional<span<uint8_t>> h4_buff = ReserveH4Buff();
    if (!h4_buff) {
      PW_LOG_WARN("No H4 buffers available. So will not send.");
      acl_send_mutex_.unlock();
      return pw::Status::Unavailable();
    }

    size_t acl_packet_size =
        emboss::AttNotifyOverAcl::MinSizeInBytes() + attribute_value.size();
    size_t h4_packet_size = sizeof(emboss::H4PacketType) + acl_packet_size;

    H4PacketWithH4 h4_temp(
        span(h4_buff->data(), h4_packet_size),
        /*release_fn=*/[this](const uint8_t* buffer) {
          acl_send_mutex_.lock();
          PW_CHECK(h4_buff_occupied_.contains(const_cast<uint8_t*>(buffer)),
                   "Received release callback for invalid buffer address.");
          h4_buff_occupied_.at(const_cast<uint8_t*>(buffer)) = false;
          acl_send_mutex_.unlock();
        });
    h4_temp.SetH4Type(emboss::H4PacketType::ACL_DATA);

    if (h4_packet_size > h4_buff->size()) {
      PW_LOG_ERROR("Buffer is too small for H4 packet. So will not send.");
      acl_send_mutex_.unlock();
      return pw::Status::InvalidArgument();
    }

    emboss::AttNotifyOverAclWriter att_notify =
        emboss::MakeAttNotifyOverAclView(attribute_value.size(),
                                         H4HciSubspan(*h4_buff).data(),
                                         acl_packet_size);
    if (!att_notify.IsComplete()) {
      PW_LOG_ERROR("Buffer is too small for ATT Notify. So will not send.");
      acl_send_mutex_.unlock();
      return pw::Status::InvalidArgument();
    }

    BuildAttNotify(
        att_notify, connection_handle, attribute_handle, attribute_value);

    h4_att_notify = std::move(h4_temp);
    acl_send_mutex_.unlock();
  }

  // H4 packet is hereby moved. Either ACL data channel will move packet to
  // controller or will be unable to send packet. In either case, packet will be
  // destructed, so its release function will clear the corresponding flag in
  // `h4_buff_occupied_`.
  if (!acl_data_channel_.SendAcl(std::move(h4_att_notify))) {
    // SendAcl function will have logged reason for failure to send..
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
