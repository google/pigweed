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
#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/gatt_notify_channel_internal.h"
#include "pw_bluetooth_proxy/internal/l2cap_coc_internal.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

ProxyHost::ProxyHost(
    pw::Function<void(H4PacketWithHci&& packet)>&& send_to_host_fn,
    pw::Function<void(H4PacketWithH4&& packet)>&& send_to_controller_fn,
    uint16_t le_acl_credits_to_reserve,
    uint16_t br_edr_acl_credits_to_reserve)
    : hci_transport_(std::move(send_to_host_fn),
                     std::move(send_to_controller_fn)),
      acl_data_channel_(hci_transport_,
                        l2cap_channel_manager_,
                        le_acl_credits_to_reserve,
                        br_edr_acl_credits_to_reserve),
      l2cap_channel_manager_(acl_data_channel_) {
  PW_LOG_INFO(
      "btproxy: ProxyHost ctor - le_acl_credits_to_reserve: %u, "
      "br_edr_acl_credits_to_reserve: %u",
      le_acl_credits_to_reserve,
      br_edr_acl_credits_to_reserve);
}

ProxyHost::~ProxyHost() {
  PW_LOG_INFO("btproxy: ProxyHost dtor");
  acl_data_channel_.Reset();
  l2cap_channel_manager_.DeregisterAndCloseChannels(
      L2capChannelEvent::kChannelClosedByOther);
}

void ProxyHost::Reset() {
  // Reset AclDataChannel first, so that send credits are reset to 0 until
  // reinitialized by controller event. This way, new channels can still be
  // registered, but they cannot erroneously use invalidated send credits.
  acl_data_channel_.Reset();
  l2cap_channel_manager_.DeregisterAndCloseChannels(L2capChannelEvent::kReset);
}

void ProxyHost::HandleH4HciFromHost(H4PacketWithH4&& h4_packet) {
  switch (h4_packet.GetH4Type()) {
    case emboss::H4PacketType::COMMAND:
      HandleCommandFromHost(std::move(h4_packet));
      return;
    case emboss::H4PacketType::EVENT:
      HandleEventFromHost(std::move(h4_packet));
      return;
    case emboss::H4PacketType::ACL_DATA:
      HandleAclFromHost(std::move(h4_packet));
      return;
    case emboss::H4PacketType::UNKNOWN:
    case emboss::H4PacketType::SYNC_DATA:
    case emboss::H4PacketType::ISO_DATA:
    default:
      hci_transport_.SendToController(std::move(h4_packet));
      return;
  }
}

void ProxyHost::HandleH4HciFromController(H4PacketWithHci&& h4_packet) {
  switch (h4_packet.GetH4Type()) {
    case emboss::H4PacketType::EVENT:
      HandleEventFromController(std::move(h4_packet));
      return;
    case emboss::H4PacketType::ACL_DATA:
      HandleAclFromController(std::move(h4_packet));
      return;
    case emboss::H4PacketType::UNKNOWN:
    case emboss::H4PacketType::COMMAND:
    case emboss::H4PacketType::SYNC_DATA:
    case emboss::H4PacketType::ISO_DATA:
    default:
      hci_transport_.SendToHost(std::move(h4_packet));
      return;
  }
}

void ProxyHost::HandleEventFromController(H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::EventHeaderView> event =
      MakeEmbossView<emboss::EventHeaderView>(hci_buffer);
  if (!event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for EventHeader. So will pass on to host without "
        "processing.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (event->event_code().Read()) {
    case emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS: {
      acl_data_channel_.HandleNumberOfCompletedPacketsEvent(
          std::move(h4_packet));
      break;
    }
    case emboss::EventCode::DISCONNECTION_COMPLETE: {
      acl_data_channel_.ProcessDisconnectionCompleteEvent(
          h4_packet.GetHciSpan());
      hci_transport_.SendToHost(std::move(h4_packet));
      break;
    }
    case emboss::EventCode::COMMAND_COMPLETE: {
      HandleCommandCompleteEvent(std::move(h4_packet));
      break;
    }
    case emboss::EventCode::CONNECTION_COMPLETE: {
      acl_data_channel_.HandleConnectionCompleteEvent(std::move(h4_packet));
      break;
    }
    case emboss::EventCode::LE_META_EVENT: {
      HandleLeMetaEvent(std::move(h4_packet));
      break;
    }
    default: {
      hci_transport_.SendToHost(std::move(h4_packet));
      return;
    }
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

void ProxyHost::HandleEventFromHost(H4PacketWithH4&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::EventHeaderView> event =
      MakeEmbossView<emboss::EventHeaderView>(hci_buffer);
  if (!event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for EventHeader. So will pass on to controller "
        "without processing.");
    hci_transport_.SendToController(std::move(h4_packet));
    return;
  }

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (event->event_code().Read()) {
    case emboss::EventCode::DISCONNECTION_COMPLETE: {
      acl_data_channel_.ProcessDisconnectionCompleteEvent(
          h4_packet.GetHciSpan());
      hci_transport_.SendToController(std::move(h4_packet));
      break;
    }
    default: {
      hci_transport_.SendToController(std::move(h4_packet));
      return;
    }
  }
  PW_MODIFY_DIAGNOSTICS_POP();
}

void ProxyHost::HandleAclFromController(H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_buffer);
  if (!acl.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for ACL header. So will pass on to host.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  if (!acl_data_channel_.HandleAclData(
          AclDataChannel::Direction::kFromController, *acl)) {
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }
}

void ProxyHost::HandleLeMetaEvent(H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::LEMetaEventView> le_meta_event_view =
      MakeEmbossView<emboss::LEMetaEventView>(hci_buffer);
  if (!le_meta_event_view.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for LE_META_EVENT event. So will not process.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (le_meta_event_view->subevent_code_enum().Read()) {
    case emboss::LeSubEventCode::CONNECTION_COMPLETE: {
      acl_data_channel_.HandleLeConnectionCompleteEvent(std::move(h4_packet));
      return;
    }
    case emboss::LeSubEventCode::ENHANCED_CONNECTION_COMPLETE_V1: {
      acl_data_channel_.HandleLeEnhancedConnectionCompleteV1Event(
          std::move(h4_packet));
      return;
    }
    case emboss::LeSubEventCode::ENHANCED_CONNECTION_COMPLETE_V2: {
      acl_data_channel_.HandleLeEnhancedConnectionCompleteV2Event(
          std::move(h4_packet));
      return;
    }
    default:
      break;
  }
  PW_MODIFY_DIAGNOSTICS_POP();
  hci_transport_.SendToHost(std::move(h4_packet));
}

void ProxyHost::HandleCommandCompleteEvent(H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::CommandCompleteEventView> command_complete_event =
      MakeEmbossView<emboss::CommandCompleteEventView>(hci_buffer);
  if (!command_complete_event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for COMMAND_COMPLETE event. So will not process.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  switch (command_complete_event->command_opcode().Read()) {
    case emboss::OpCode::READ_BUFFER_SIZE: {
      Result<emboss::ReadBufferSizeCommandCompleteEventWriter> read_event =
          MakeEmbossWriter<emboss::ReadBufferSizeCommandCompleteEventWriter>(
              hci_buffer);
      if (!read_event.ok()) {
        PW_LOG_ERROR(
            "Buffer is too small for READ_BUFFER_SIZE command complete event. "
            "Will not process.");
        break;
      }
      acl_data_channel_.ProcessReadBufferSizeCommandCompleteEvent(*read_event);
      break;
    }
    case emboss::OpCode::LE_READ_BUFFER_SIZE_V1: {
      Result<emboss::LEReadBufferSizeV1CommandCompleteEventWriter> read_event =
          MakeEmbossWriter<
              emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(hci_buffer);
      if (!read_event.ok()) {
        PW_LOG_ERROR(
            "Buffer is too small for LE_READ_BUFFER_SIZE_V1 command complete "
            "event. So will not process.");
        break;
      }
      acl_data_channel_.ProcessLEReadBufferSizeCommandCompleteEvent(
          *read_event);
      break;
    }
    case emboss::OpCode::LE_READ_BUFFER_SIZE_V2: {
      Result<emboss::LEReadBufferSizeV2CommandCompleteEventWriter> read_event =
          MakeEmbossWriter<
              emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(hci_buffer);
      if (!read_event.ok()) {
        PW_LOG_ERROR(
            "Buffer is too small for LE_READ_BUFFER_SIZE_V2 command complete "
            "event. So will not process.");
        break;
      }
      acl_data_channel_.ProcessLEReadBufferSizeCommandCompleteEvent(
          *read_event);
      break;
    }
    default:
      break;
  }
  PW_MODIFY_DIAGNOSTICS_POP();
  hci_transport_.SendToHost(std::move(h4_packet));
}

void ProxyHost::HandleCommandFromHost(H4PacketWithH4&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::GenericHciCommandView> command =
      MakeEmbossView<emboss::GenericHciCommandView>(hci_buffer);
  if (!command.ok()) {
    hci_transport_.SendToController(std::move(h4_packet));
    return;
  }

  if (command->header().opcode().Read() == emboss::OpCode::RESET) {
    PW_LOG_INFO("Resetting proxy on HCI_Reset Command from host.");
    Reset();
  }

  hci_transport_.SendToController(std::move(h4_packet));
}

void ProxyHost::HandleAclFromHost(H4PacketWithH4&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_buffer);
  if (!acl.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for ACL header. So will pass on to controller.");
    hci_transport_.SendToController(std::move(h4_packet));
    return;
  }

  if (!acl_data_channel_.HandleAclData(AclDataChannel::Direction::kFromHost,
                                       *acl)) {
    hci_transport_.SendToController(std::move(h4_packet));
    return;
  }
}

pw::Result<L2capCoc> ProxyHost::AcquireL2capCoc(
    pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
    uint16_t connection_handle,
    L2capCoc::CocConfig rx_config,
    L2capCoc::CocConfig tx_config,
    Function<void(multibuf::MultiBuf&& payload)>&& receive_fn,
    ChannelEventCallback&& event_fn) {
  Status status = acl_data_channel_.CreateAclConnection(connection_handle,
                                                        AclTransportType::kLe);
  if (status.IsResourceExhausted()) {
    return pw::Status::Unavailable();
  }
  PW_CHECK(status.ok() || status.IsAlreadyExists());

  L2capSignalingChannel* signaling_channel =
      acl_data_channel_.FindSignalingChannel(
          connection_handle,
          static_cast<uint16_t>(emboss::L2capFixedCid::LE_U_SIGNALING));
  PW_CHECK(signaling_channel);
  return L2capCocInternal::Create(rx_multibuf_allocator,
                                  l2cap_channel_manager_,
                                  signaling_channel,
                                  connection_handle,
                                  rx_config,
                                  tx_config,
                                  std::move(event_fn),
                                  /*receive_fn=*/std::move(receive_fn));
}

pw::Status ProxyHost::SendAdditionalRxCredits(uint16_t connection_handle,
                                              uint16_t local_cid,
                                              uint16_t additional_rx_credits) {
  std::optional<L2capChannelManager::LockedL2capChannel> channel =
      l2cap_channel_manager_.FindChannelByLocalCid(connection_handle,
                                                   local_cid);
  PW_CHECK(channel.has_value());
  return static_cast<L2capCoc&>(channel->channel())
      .SendAdditionalRxCredits(additional_rx_credits);
}

pw::Result<BasicL2capChannel> ProxyHost::AcquireBasicL2capChannel(
    multibuf::MultiBufAllocator& rx_multibuf_allocator,
    uint16_t connection_handle,
    uint16_t local_cid,
    uint16_t remote_cid,
    AclTransportType transport,
    OptionalPayloadReceiveCallback&& payload_from_controller_fn,
    OptionalPayloadReceiveCallback&& payload_from_host_fn,
    ChannelEventCallback&& event_fn) {
  Status status =
      acl_data_channel_.CreateAclConnection(connection_handle, transport);
  if (status.IsResourceExhausted()) {
    return pw::Status::Unavailable();
  }
  PW_CHECK(status.ok() || status.IsAlreadyExists());
  return BasicL2capChannel::Create(l2cap_channel_manager_,
                                   &rx_multibuf_allocator,
                                   /*connection_handle=*/connection_handle,
                                   /*transport=*/transport,
                                   /*local_cid=*/local_cid,
                                   /*remote_cid=*/remote_cid,
                                   /*payload_from_controller_fn=*/
                                   std::move(payload_from_controller_fn),
                                   /*payload_from_host_fn=*/
                                   std::move(payload_from_host_fn),
                                   /*event_fn=*/std::move(event_fn));
}

pw::Result<GattNotifyChannel> ProxyHost::AcquireGattNotifyChannel(
    int16_t connection_handle,
    uint16_t attribute_handle,
    ChannelEventCallback&& event_fn) {
  Status status = acl_data_channel_.CreateAclConnection(connection_handle,
                                                        AclTransportType::kLe);
  if (status != OkStatus() && status != Status::AlreadyExists()) {
    return pw::Status::Unavailable();
  }
  return GattNotifyChannelInternal::Create(l2cap_channel_manager_,
                                           connection_handle,
                                           attribute_handle,
                                           std::move(event_fn));
}

StatusWithMultiBuf ProxyHost::SendGattNotify(uint16_t connection_handle,
                                             uint16_t attribute_handle,
                                             pw::multibuf::MultiBuf&& payload) {
  // TODO: https://pwbug.dev/369709521 - Migrate clients to channel API.
  pw::Result<GattNotifyChannel> channel_result =
      AcquireGattNotifyChannel(connection_handle, attribute_handle, nullptr);
  if (!channel_result.ok()) {
    return {channel_result.status(), std::move(payload)};
  }
  return channel_result->Write(std::move(payload));
}

pw::Status ProxyHost::SendGattNotify(uint16_t connection_handle,
                                     uint16_t attribute_handle,
                                     pw::span<const uint8_t> attribute_value) {
  // TODO: https://pwbug.dev/369709521 - Migrate clients to channel API.
  pw::Result<GattNotifyChannel> channel_result =
      AcquireGattNotifyChannel(connection_handle, attribute_handle, nullptr);
  if (!channel_result.ok()) {
    return channel_result.status();
  }
  return channel_result->Write(attribute_value);
}

pw::Result<RfcommChannel> ProxyHost::AcquireRfcommChannel(
    multibuf::MultiBufAllocator& rx_multibuf_allocator,
    uint16_t connection_handle,
    RfcommChannel::Config rx_config,
    RfcommChannel::Config tx_config,
    uint8_t channel_number,
    Function<void(multibuf::MultiBuf&& payload)>&& payload_from_controller_fn,
    ChannelEventCallback&& event_fn) {
  Status status = acl_data_channel_.CreateAclConnection(
      connection_handle, AclTransportType::kBrEdr);
  if (status != OkStatus() && status != Status::AlreadyExists()) {
    return pw::Status::Unavailable();
  }
  return RfcommChannel::Create(l2cap_channel_manager_,
                               rx_multibuf_allocator,
                               connection_handle,
                               rx_config,
                               tx_config,
                               channel_number,
                               std::move(payload_from_controller_fn),
                               std::move(event_fn));
}

bool ProxyHost::HasSendLeAclCapability() const {
  return acl_data_channel_.HasSendAclCapability(AclTransportType::kLe);
}

bool ProxyHost::HasSendBrEdrAclCapability() const {
  return acl_data_channel_.HasSendAclCapability(AclTransportType::kBrEdr);
}

uint16_t ProxyHost::GetNumFreeLeAclPackets() const {
  return acl_data_channel_.GetNumFreeAclPackets(AclTransportType::kLe);
}

uint16_t ProxyHost::GetNumFreeBrEdrAclPackets() const {
  return acl_data_channel_.GetNumFreeAclPackets(AclTransportType::kBrEdr);
}

void ProxyHost::RegisterL2capStatusDelegate(L2capStatusDelegate& delegate) {
  l2cap_channel_manager_.RegisterStatusDelegate(delegate);
}

void ProxyHost::UnregisterL2capStatusDelegate(L2capStatusDelegate& delegate) {
  l2cap_channel_manager_.UnregisterStatusDelegate(delegate);
}

}  // namespace pw::bluetooth::proxy
