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

bool ProxyHost::CheckForActiveFragmenting(AclDataChannel::Direction direction,
                                          emboss::AclDataFrameWriter& acl) {
  const uint16_t handle = acl.header().handle().Read();
  const emboss::AclDataPacketBoundaryFlag boundary_flag =
      acl.header().packet_boundary_flag().Read();

  pw::Result<bool> connection_is_receiving_fragmented_pdu =
      acl_data_channel_.IsReceivingFragmentedPdu(direction, handle);
  if (connection_is_receiving_fragmented_pdu.ok() &&
      *connection_is_receiving_fragmented_pdu) {
    // We're in a state where this connection is dropping continuing fragments
    // in a fragmented PDU.
    if (boundary_flag !=
        emboss::AclDataPacketBoundaryFlag::CONTINUING_FRAGMENT) {
      // The fragmented PDU has been fully received, so note that then proceed
      // to process the new PDU as normal.
      PW_CHECK(acl_data_channel_.FragmentedPduFinished(direction, handle).ok());
    } else {
      PW_LOG_INFO("(Connection: 0x%X) Dropping continuing PDU fragment.",
                  handle);
      return true;
    }
  }
  return false;
}

bool ProxyHost::CheckForFragmentedStart(
    AclDataChannel::Direction direction,
    emboss::AclDataFrameWriter& acl,
    emboss::BasicL2capHeaderView& l2cap_header,
    L2capChannel* channel) {
  const uint16_t handle = acl.header().handle().Read();
  const emboss::AclDataPacketBoundaryFlag boundary_flag =
      acl.header().packet_boundary_flag().Read();
  // TODO: https://pwbug.dev/365179076 - Support recombination.
  if (boundary_flag == emboss::AclDataPacketBoundaryFlag::CONTINUING_FRAGMENT) {
    PW_LOG_INFO("(CID: 0x%X) Received unexpected continuing PDU fragment.",
                handle);
    channel->HandleFragmentedPdu();
    return true;
  }
  const uint16_t l2cap_frame_length =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
      l2cap_header.pdu_length().Read();
  if (l2cap_frame_length > acl.data_total_length().Read()) {
    pw::Status status =
        acl_data_channel_.FragmentedPduStarted(direction, handle);
    PW_CHECK(status.ok());
    channel->HandleFragmentedPdu();
    return true;
  }

  return false;
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
  const uint16_t handle = acl->header().handle().Read();

  if (CheckForActiveFragmenting(AclDataChannel::Direction::kFromController,
                                *acl)) {
    return;
  }

  emboss::BasicL2capHeaderView l2cap_header = emboss::MakeBasicL2capHeaderView(
      acl->payload().BackingStorage().data(),
      acl->payload().BackingStorage().SizeInBytes());
  // TODO: https://pwbug.dev/365179076 - Technically, the first fragment of a
  // fragmented PDU may include an incomplete L2CAP header.
  if (!l2cap_header.Ok()) {
    PW_LOG_ERROR(
        "(Connection: 0x%X) ACL packet does not include a valid L2CAP header. "
        "So will pass on to host.",
        handle);
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  L2capChannel* channel = l2cap_channel_manager_.FindChannelByLocalCid(
      acl->header().handle().Read(), l2cap_header.channel_id().Read());
  if (!channel) {
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  if (CheckForFragmentedStart(AclDataChannel::Direction::kFromController,
                              *acl,
                              l2cap_header,
                              channel)) {
    return;
  }

  if (!channel->HandlePduFromController(
          pw::span(acl->payload().BackingStorage().data(),
                   acl->payload().SizeInBytes()))) {
    hci_transport_.SendToHost(std::move(h4_packet));
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

  // TODO: https://pwbug.dev/381902130 - Handle reset on command complete
  // successful instead. Also event to container on reset.
  if (command->header().opcode().Read() == emboss::OpCode::RESET) {
    PW_LOG_INFO("Resetting proxy on seeing RESET command.");
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

  if (CheckForActiveFragmenting(AclDataChannel::Direction::kFromHost, *acl)) {
    return;
  }

  const uint16_t handle = acl->header().handle().Read();
  emboss::BasicL2capHeaderView l2cap_header = emboss::MakeBasicL2capHeaderView(
      acl->payload().BackingStorage().data(),
      acl->payload().BackingStorage().SizeInBytes());
  // TODO: https://pwbug.dev/365179076 - Technically, the first fragment of a
  // fragmented PDU may include an incomplete L2CAP header.
  if (!l2cap_header.Ok()) {
    PW_LOG_ERROR(
        "(Connection: 0x%X) ACL packet does not include a valid L2CAP header. "
        "So will pass on to controller.",
        handle);
    hci_transport_.SendToController(std::move(h4_packet));
    return;
  }

  L2capChannel* channel = l2cap_channel_manager_.FindChannelByRemoteCid(
      acl->header().handle().Read(), l2cap_header.channel_id().Read());
  if (!channel) {
    hci_transport_.SendToController(std::move(h4_packet));
    return;
  }

  if (CheckForFragmentedStart(
          AclDataChannel::Direction::kFromHost, *acl, l2cap_header, channel)) {
    return;
  }

  if (!channel->HandlePduFromHost(
          pw::span(acl->payload().BackingStorage().data(),
                   acl->payload().SizeInBytes()))) {
    hci_transport_.SendToController(std::move(h4_packet));
  }
}

void ProxyHost::Reset() {
  acl_data_channel_.Reset();
  l2cap_channel_manager_.Reset();
}

pw::Result<L2capCoc> ProxyHost::AcquireL2capCoc(
    pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
    uint16_t connection_handle,
    L2capCoc::CocConfig rx_config,
    L2capCoc::CocConfig tx_config,
    Function<void(multibuf::MultiBuf&& payload)>&& receive_fn,
    Function<void(L2capChannelEvent event)>&& event_fn) {
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
  return L2capCocInternal::Create(
      rx_multibuf_allocator,
      l2cap_channel_manager_,
      signaling_channel,
      connection_handle,
      rx_config,
      tx_config,
      /*receive_fn=*/nullptr,
      std::move(event_fn),
      /*receive_fn_multibuf=*/std::move(receive_fn));
}

// TODO: https://pwbug.dev/379337272 - Remove once clients move to new signature
// with allocators.
pw::Result<L2capCoc> ProxyHost::AcquireL2capCoc(
    pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
    uint16_t connection_handle,
    L2capCoc::CocConfig rx_config,
    L2capCoc::CocConfig tx_config,
    Function<void(pw::span<uint8_t> payload)>&& receive_fn,
    Function<void(L2capChannelEvent event)>&& event_fn) {
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
  // Create config using the temporary shared allocators.
  return L2capCocInternal::Create(rx_multibuf_allocator,
                                  l2cap_channel_manager_,
                                  signaling_channel,
                                  connection_handle,
                                  rx_config,
                                  tx_config,
                                  /*receive_fn=*/std::move(receive_fn),
                                  std::move(event_fn),
                                  /*receive_fn_multibuf=*/nullptr);
}

pw::Status ProxyHost::SendAdditionalRxCredits(uint16_t connection_handle,
                                              uint16_t local_cid,
                                              uint16_t additional_rx_credits) {
  L2capChannel* channel = l2cap_channel_manager_.FindChannelByLocalCid(
      connection_handle, local_cid);
  PW_CHECK(channel);
  return static_cast<L2capCoc*>(channel)->SendAdditionalRxCredits(
      additional_rx_credits);
}

pw::Result<BasicL2capChannel> ProxyHost::AcquireBasicL2capChannel(
    uint16_t connection_handle,
    uint16_t local_cid,
    uint16_t remote_cid,
    AclTransportType transport,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_host_fn,
    Function<void(L2capChannelEvent event)>&& event_fn) {
  Status status =
      acl_data_channel_.CreateAclConnection(connection_handle, transport);
  if (status.IsResourceExhausted()) {
    return pw::Status::Unavailable();
  }
  PW_CHECK(status.ok() || status.IsAlreadyExists());
  return BasicL2capChannel::Create(
      /*l2cap_channel_manager=*/l2cap_channel_manager_,
      /*connection_handle=*/connection_handle,
      /*transport=*/transport,
      /*local_cid=*/local_cid,
      /*remote_cid=*/remote_cid,
      /*payload_from_controller_fn=*/std::move(payload_from_controller_fn),
      /*payload_from_host_fn=*/std::move(payload_from_host_fn),
      /*event_fn=*/std::move(event_fn));
}

pw::Result<BasicL2capChannel> ProxyHost::AcquireBasicL2capChannel(
    uint16_t connection_handle,
    uint16_t local_cid,
    uint16_t remote_cid,
    AclTransportType transport,
    Function<bool(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<void(L2capChannelEvent event)>&& event_fn) {
  return AcquireBasicL2capChannel(connection_handle,
                                  local_cid,
                                  remote_cid,
                                  transport,
                                  std::move(payload_from_controller_fn),
                                  nullptr,
                                  std::move(event_fn));
}

namespace {

pw::Result<GattNotifyChannel> CreateGattNotifyChannel(
    AclDataChannel& acl_data_channel,
    L2capChannelManager& l2cap_channel_manager,
    uint16_t connection_handle,
    uint16_t attribute_handle) {
  Status status = acl_data_channel.CreateAclConnection(connection_handle,
                                                       AclTransportType::kLe);
  if (status != OkStatus() && status != Status::AlreadyExists()) {
    return pw::Status::Unavailable();
  }
  return GattNotifyChannelInternal::Create(
      l2cap_channel_manager, connection_handle, attribute_handle);
}
}  // namespace

StatusWithMultiBuf ProxyHost::SendGattNotify(uint16_t connection_handle,
                                             uint16_t attribute_handle,
                                             pw::multibuf::MultiBuf&& payload) {
  // TODO: https://pwbug.dev/369709521 - Migrate clients to channel API.
  pw::Result<GattNotifyChannel> channel_result =
      CreateGattNotifyChannel(acl_data_channel_,
                              l2cap_channel_manager_,
                              connection_handle,
                              attribute_handle);
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
      CreateGattNotifyChannel(acl_data_channel_,
                              l2cap_channel_manager_,
                              connection_handle,
                              attribute_handle);
  if (!channel_result.ok()) {
    return channel_result.status();
  }
  return channel_result->Write(attribute_value);
}

pw::Result<RfcommChannel> ProxyHost::AcquireRfcommChannel(
    uint16_t connection_handle,
    RfcommChannel::Config rx_config,
    RfcommChannel::Config tx_config,
    uint8_t channel_number,
    Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
    Function<void(L2capChannelEvent event)>&& event_fn) {
  Status status = acl_data_channel_.CreateAclConnection(
      connection_handle, AclTransportType::kBrEdr);
  if (status != OkStatus() && status != Status::AlreadyExists()) {
    return pw::Status::Unavailable();
  }
  return RfcommChannel::Create(l2cap_channel_manager_,
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
