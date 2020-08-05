// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_signaling_server.h"

#include <endian.h>

#include "fake_l2cap.h"

namespace bt {
namespace testing {

FakeSignalingServer::FakeSignalingServer(SendFrameCallback send_frame_callback)
    : send_frame_callback_(std::move(send_frame_callback)) {}

void FakeSignalingServer::RegisterWithL2cap(FakeL2cap* l2cap_) {
  auto cb = [&](auto conn, auto& sdu) { return HandleSdu(conn, sdu); };
  l2cap_->RegisterHandler(l2cap::kSignalingChannelId, cb);
  fake_l2cap_ = l2cap_;
  return;
}

void FakeSignalingServer::HandleSdu(hci::ConnectionHandle conn, const ByteBuffer& sdu) {
  ZX_ASSERT_MSG(sdu.size() >= sizeof(l2cap::CommandHeader), "SDU has only %zu bytes", sdu.size());
  ZX_ASSERT(sdu.As<l2cap::CommandHeader>().length == (sdu.size() - sizeof(l2cap::CommandHeader)));
  // Extract CommandCode and strip signaling packet header from sdu.
  l2cap::CommandHeader packet_header = sdu.As<l2cap::CommandHeader>();
  l2cap::CommandCode packet_code = packet_header.code;
  l2cap::CommandId packet_id = packet_header.id;
  auto payload = sdu.view(sizeof(l2cap::CommandHeader));
  switch (packet_code) {
    case l2cap::kInformationRequest:
      ProcessInformationRequest(conn, packet_id, payload);
      break;
    case l2cap::kConnectionRequest:
      ProcessConnectionRequest(conn, packet_id, payload);
      break;
    case l2cap::kConfigurationRequest:
      ProcessConfigurationRequest(conn, packet_id, payload);
      break;
    case l2cap::kConfigurationResponse:
      ProcessConfigurationResponse(conn, packet_id, payload);
      break;
    case l2cap::kDisconnectionRequest:
      ProcessDisconnectionRequest(conn, packet_id, payload);
      break;
    default:
      bt_log(ERROR, "hci-emulator", "Does not support request code: %#.4hx", packet_code);
      break;
  }
}

void FakeSignalingServer::ProcessInformationRequest(hci::ConnectionHandle conn, l2cap::CommandId id,
                                                    const ByteBuffer& info_req) {
  auto info_type = info_req.As<l2cap::InformationType>();
  if (info_type == l2cap::InformationType::kExtendedFeaturesSupported) {
    SendInformationResponseExtendedFeatures(conn, id);
  } else if (info_type == l2cap::InformationType::kFixedChannelsSupported) {
    SendInformationResponseFixedChannels(conn, id);
  } else {
    SendCommandReject(conn, id, l2cap::RejectReason::kNotUnderstood);
  }
}

void FakeSignalingServer::ProcessConnectionRequest(hci::ConnectionHandle conn, l2cap::CommandId id,
                                                   const ByteBuffer& connection_req) {
  const auto& conn_req = connection_req.As<l2cap::ConnectionRequestPayload>();
  const l2cap::PSM psm = letoh16(conn_req.psm);
  const l2cap::ChannelId remote_cid = letoh16(conn_req.src_cid);

  // Validate the remote channel ID prior to assigning it a local ID.
  if (remote_cid == l2cap::kInvalidChannelId) {
    bt_log(ERROR, "hci-emulator",
           "Invalid source CID; rejecting connection for PSM %#.4x from channel %#.4x", psm,
           remote_cid);
    SendConnectionResponse(conn, id, l2cap::kInvalidChannelId, remote_cid,
                           l2cap::ConnectionResult::kInvalidSourceCID,
                           l2cap::ConnectionStatus::kNoInfoAvailable);
    return;
  }
  if (fake_l2cap_->FindDynamicChannelByRemoteId(conn, remote_cid)) {
    bt_log(ERROR, "l2cap-bredr",
           "Remote CID already in use; rejecting connection for PSM %#.4x from channel %#.4x", psm,
           remote_cid);
    SendConnectionResponse(conn, id, l2cap::kInvalidChannelId, remote_cid,
                           l2cap::ConnectionResult::kSourceCIDAlreadyAllocated,
                           l2cap::ConnectionStatus::kNoInfoAvailable);
    return;
  }
  if (fake_l2cap_->ServiceRegisteredForPsm(psm) == false) {
    bt_log(ERROR, "l2cap-bredr", "No service registered for PSM %#.4x", psm);
    SendConnectionResponse(conn, id, l2cap::kInvalidChannelId, remote_cid,
                           l2cap::ConnectionResult::kPSMNotSupported,
                           l2cap::ConnectionStatus::kNoInfoAvailable);
    return;
  }

  // Assign the new channel a local channel ID.
  l2cap::ChannelId local_cid = fake_l2cap_->FindAvailableDynamicChannelId(conn);
  if (local_cid == l2cap::kInvalidChannelId) {
    bt_log(DEBUG, "l2cap-bredr",
           "Out of IDs; rejecting connection for PSM %#.4x from channel %#.4x", psm, remote_cid);
    SendConnectionResponse(conn, id, local_cid, remote_cid, l2cap::ConnectionResult::kNoResources,
                           l2cap::ConnectionStatus::kNoInfoAvailable);
    return;
  }

  // Send a ConnectionResponse and ConfigurationRequest, and then register the channel.
  SendConnectionResponse(conn, id, local_cid, remote_cid, l2cap::ConnectionResult::kSuccess,
                         l2cap::ConnectionStatus::kNoInfoAvailable);
  SendConfigurationRequest(conn, id, remote_cid);
  fake_l2cap_->RegisterDynamicChannel(conn, psm, local_cid, remote_cid);
}

void FakeSignalingServer::ProcessConfigurationRequest(hci::ConnectionHandle conn,
                                                      l2cap::CommandId id,
                                                      const ByteBuffer& configuration_req) {
  // Ignore the data/flags associated with the request for the purpose of the emulator
  const auto& conf_req = configuration_req.As<l2cap::ConfigurationRequestPayload>();
  const l2cap::ChannelId local_cid = letoh16(conf_req.dst_cid);
  auto channel = fake_l2cap_->FindDynamicChannelByLocalId(conn, local_cid);
  if (channel) {
    channel->set_configuration_request_received();
    SendConfigurationResponse(conn, id, local_cid, l2cap::ConfigurationResult::kSuccess);
    if (channel->configuration_response_received() && channel->configuration_request_received()) {
      channel->set_opened();
      fake_l2cap_->RegisterDynamicChannelWithPsm(conn, local_cid);
    }
  } else {
    bt_log(ERROR, "fake-hci", "No local channel at channel ID %#.4x", local_cid);
    SendConfigurationResponse(conn, id, local_cid, l2cap::ConfigurationResult::kRejected);
  }
  return;
}

void FakeSignalingServer::ProcessConfigurationResponse(hci::ConnectionHandle conn,
                                                       l2cap::CommandId id,
                                                       const ByteBuffer& configuration_res) {
  const auto& conf_res = configuration_res.As<l2cap::ConfigurationResponsePayload>();
  const l2cap::ChannelId local_cid = letoh16(conf_res.src_cid);
  const l2cap::ConfigurationResult result = conf_res.result;
  if (result != l2cap::ConfigurationResult::kSuccess) {
    bt_log(ERROR, "fake-hci", "Failed to create local channel at channel ID %#.4x", local_cid);
  }
  auto channel = fake_l2cap_->FindDynamicChannelByLocalId(conn, local_cid);
  if (channel) {
    channel->set_configuration_response_received();
    if (channel->configuration_response_received() && channel->configuration_request_received()) {
      channel->set_opened();
      fake_l2cap_->RegisterDynamicChannelWithPsm(conn, local_cid);
    }
  }
}

void FakeSignalingServer::ProcessDisconnectionRequest(hci::ConnectionHandle conn,
                                                      l2cap::CommandId id,
                                                      const ByteBuffer& disconnection_req) {
  const auto& disconn_req = disconnection_req.As<l2cap::DisconnectionRequestPayload>();
  const l2cap::ChannelId local_cid = letoh16(disconn_req.dst_cid);
  const l2cap::ChannelId remote_cid = letoh16(disconn_req.src_cid);
  auto channel = fake_l2cap_->FindDynamicChannelByLocalId(conn, local_cid);
  if (channel) {
    fake_l2cap_->DeleteDynamicChannelByLocalId(conn, local_cid);
    return SendDisconnectionResponse(conn, id, local_cid, remote_cid);
  } else {
    bt_log(ERROR, "fake-hci",
           "Received disconnection request for non-existent local channel at %#.4x", local_cid);
  }
}

void FakeSignalingServer::SendCFrame(hci::ConnectionHandle conn, l2cap::CommandCode code,
                                     l2cap::CommandId id, DynamicByteBuffer& payload_buffer) {
  size_t kResponseSize = sizeof(l2cap::CommandHeader) + payload_buffer.size();
  DynamicByteBuffer response_buffer(kResponseSize);
  MutablePacketView<l2cap::CommandHeader> command_packet(&response_buffer, payload_buffer.size());
  command_packet.mutable_header()->code = code;
  command_packet.mutable_header()->id = id;
  command_packet.mutable_header()->length = payload_buffer.size();
  command_packet.mutable_payload_data().Write(payload_buffer);
  send_frame_callback_(conn, response_buffer);
}

void FakeSignalingServer::SendCommandReject(hci::ConnectionHandle conn, l2cap::CommandId id,
                                            l2cap::RejectReason reason) {
  DynamicByteBuffer payload_buffer(sizeof(l2cap::CommandRejectPayload));
  MutablePacketView<l2cap::CommandRejectPayload> payload_view(&payload_buffer);
  payload_view.mutable_header()->reason =
      static_cast<uint16_t>(l2cap::RejectReason::kNotUnderstood);
  SendCFrame(conn, l2cap::kCommandRejectCode, id, payload_buffer);
}

void FakeSignalingServer::SendInformationResponseExtendedFeatures(hci::ConnectionHandle conn,
                                                                  l2cap::CommandId id) {
  l2cap::ExtendedFeatures extended_features =
      l2cap::kExtendedFeaturesBitFixedChannels | l2cap::kExtendedFeaturesBitEnhancedRetransmission;
  constexpr size_t kPayloadSize =
      sizeof(l2cap::InformationResponsePayload) + sizeof(l2cap::ExtendedFeatures);
  DynamicByteBuffer payload_buffer(kPayloadSize);
  MutablePacketView<l2cap::InformationResponsePayload> payload_view(
      &payload_buffer, sizeof(l2cap::ExtendedFeatures));
  payload_view.mutable_header()->type = static_cast<l2cap::InformationType>(
      htole16(l2cap::InformationType::kExtendedFeaturesSupported));
  payload_view.mutable_header()->result =
      static_cast<l2cap::InformationResult>(htole16(l2cap::InformationResult::kSuccess));
  payload_view.mutable_payload_data().WriteObj(htole32(extended_features));
  SendCFrame(conn, l2cap::kInformationResponse, id, payload_buffer);
}

void FakeSignalingServer::SendInformationResponseFixedChannels(hci::ConnectionHandle conn,
                                                               l2cap::CommandId id) {
  l2cap::FixedChannelsSupported fixed_channels = l2cap::kFixedChannelsSupportedBitSignaling;
  constexpr size_t kPayloadSize =
      sizeof(l2cap::InformationResponsePayload) + sizeof(l2cap::FixedChannelsSupported);
  DynamicByteBuffer payload_buffer(kPayloadSize);
  MutablePacketView<l2cap::InformationResponsePayload> payload_view(
      &payload_buffer, sizeof(l2cap::FixedChannelsSupported));
  payload_view.mutable_header()->type =
      static_cast<l2cap::InformationType>(htole16(l2cap::InformationType::kFixedChannelsSupported));
  payload_view.mutable_header()->result =
      static_cast<l2cap::InformationResult>(htole16(l2cap::InformationResult::kSuccess));
  payload_view.mutable_payload_data().WriteObj(htole64(fixed_channels));
  SendCFrame(conn, l2cap::kInformationResponse, id, payload_buffer);
}

void FakeSignalingServer::SendConnectionResponse(hci::ConnectionHandle conn, l2cap::CommandId id,
                                                 l2cap::ChannelId local_cid,
                                                 l2cap::ChannelId remote_cid,
                                                 l2cap::ConnectionResult result,
                                                 l2cap::ConnectionStatus status) {
  DynamicByteBuffer payload_buffer(sizeof(l2cap::ConnectionResponsePayload));
  MutablePacketView<l2cap::ConnectionResponsePayload> payload_view(&payload_buffer);
  // Destination cid is the endpoint on the device sending the response packet.
  payload_view.mutable_header()->dst_cid = local_cid;
  // Source cid is the endpoint on the device receiving the response packet.
  payload_view.mutable_header()->src_cid = remote_cid;
  payload_view.mutable_header()->result = result;
  payload_view.mutable_header()->status = status;
  SendCFrame(conn, l2cap::kConnectionResponse, id, payload_buffer);
}

void FakeSignalingServer::SendConfigurationRequest(hci::ConnectionHandle conn, l2cap::CommandId id,
                                                   l2cap::ChannelId remote_cid) {
  DynamicByteBuffer payload_buffer(sizeof(l2cap::ConfigurationRequestPayload));
  MutablePacketView<l2cap::ConfigurationRequestPayload> payload_view(&payload_buffer);
  // Config request dest_cid contains channel endpoint on the device receiving the request.
  payload_view.mutable_header()->dst_cid = remote_cid;
  // No continuation flag or additional data associated with this request.
  payload_view.mutable_header()->flags = 0x0000;
  SendCFrame(conn, l2cap::kConfigurationRequest, id, payload_buffer);
}

void FakeSignalingServer::SendConfigurationResponse(hci::ConnectionHandle conn, l2cap::CommandId id,
                                                    l2cap::ChannelId local_cid,
                                                    l2cap::ConfigurationResult result) {
  DynamicByteBuffer payload_buffer(sizeof(l2cap::ConfigurationResponsePayload));
  MutablePacketView<l2cap::ConfigurationResponsePayload> payload_view(&payload_buffer);
  // Config response src_cid contains the channel endpoint on the device sending the request.
  payload_view.mutable_header()->src_cid = local_cid;
  payload_view.mutable_header()->result = result;
  // No continuation flag or additional data associated with this request.
  payload_view.mutable_header()->flags = 0x0000;
  SendCFrame(conn, l2cap::kConfigurationResponse, id, payload_buffer);
}

void FakeSignalingServer::SendDisconnectionResponse(hci::ConnectionHandle conn, l2cap::CommandId id,
                                                    l2cap::ChannelId local_cid,
                                                    l2cap::ChannelId remote_cid) {
  DynamicByteBuffer payload_buffer(sizeof(l2cap::DisconnectionResponsePayload));
  MutablePacketView<l2cap::DisconnectionResponsePayload> payload_view(&payload_buffer);
  // Endpoint on the device receiving the response.
  payload_view.mutable_header()->src_cid = remote_cid;
  // Endpoint on device sending the response.
  payload_view.mutable_header()->dst_cid = local_cid;
  SendCFrame(conn, l2cap::kDisconnectionResponse, id, payload_buffer);
}

}  // namespace testing
}  // namespace bt
