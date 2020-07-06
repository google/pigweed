// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_signaling_server.h"

#include <endian.h>

namespace bt {
namespace testing {

FakeSignalingServer::FakeSignalingServer(SendFrameCallback send_frame_callback)
    : send_frame_callback_(std::move(send_frame_callback)) {}

void FakeSignalingServer::RegisterWithL2cap(FakeL2cap* l2cap_) {
  auto cb = [&](auto conn, auto& sdu) { return HandleSdu(conn, sdu); };
  l2cap_->RegisterHandler(l2cap::kSignalingChannelId, cb);
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
    case l2cap::kInformationRequest: {
      ProcessInformationRequest(conn, packet_id, payload);
    }
    default: {
      bt_log(ERROR, "hci-emulator", "Does not support request code: %#.4hx", packet_code);
      break;
    }
  }
}

void FakeSignalingServer::ProcessInformationRequest(hci::ConnectionHandle conn, l2cap::CommandId id,
                                                    const ByteBuffer& info_req) {
  auto info_type = info_req.As<l2cap::InformationType>();
  if (info_type == l2cap::InformationType::kExtendedFeaturesSupported) {
    l2cap::ExtendedFeatures extended_features = l2cap::kExtendedFeaturesBitFixedChannels |
                                                l2cap::kExtendedFeaturesBitEnhancedRetransmission;
    constexpr size_t kPayloadSize =
        sizeof(l2cap::InformationResponsePayload) + sizeof(extended_features);
    constexpr size_t kResponseSize = sizeof(l2cap::CommandHeader) + kPayloadSize;
    StaticByteBuffer<kResponseSize> response_buffer;
    MutablePacketView<l2cap::CommandHeader> command_packet(&response_buffer, kPayloadSize);
    command_packet.mutable_header()->code = l2cap::kInformationResponse;
    command_packet.mutable_header()->id = id;
    command_packet.mutable_header()->length = htole16(kPayloadSize);
    auto* response_payload = command_packet.mutable_payload<l2cap::InformationResponsePayload>();
    response_payload->type = static_cast<l2cap::InformationType>(
        htole16(l2cap::InformationType::kExtendedFeaturesSupported));
    response_payload->result =
        static_cast<l2cap::InformationResult>(htole16(l2cap::InformationResult::kSuccess));
    MutableBufferView(response_payload->data, sizeof(extended_features))
        .WriteObj(htole32(extended_features));
    send_frame_callback_(conn, response_buffer);
  } else if (info_type == l2cap::InformationType::kFixedChannelsSupported) {
    l2cap::FixedChannelsSupported fixed_channels = l2cap::kFixedChannelsSupportedBitSignaling;
    constexpr size_t kPayloadSize =
        sizeof(l2cap::InformationResponsePayload) + sizeof(fixed_channels);
    constexpr size_t kResponseSize = sizeof(l2cap::CommandHeader) + kPayloadSize;
    StaticByteBuffer<kResponseSize> response_buffer;
    MutablePacketView<l2cap::CommandHeader> command_packet(&response_buffer, kPayloadSize);
    command_packet.mutable_header()->code = l2cap::kInformationResponse;
    command_packet.mutable_header()->id = id;
    command_packet.mutable_header()->length = htole16(kPayloadSize);
    auto* response_payload = command_packet.mutable_payload<l2cap::InformationResponsePayload>();
    response_payload->type = static_cast<l2cap::InformationType>(
        htole16(l2cap::InformationType::kFixedChannelsSupported));
    response_payload->result =
        static_cast<l2cap::InformationResult>(htole16(l2cap::InformationResult::kSuccess));
    MutableBufferView(response_payload->data, sizeof(fixed_channels))
        .WriteObj(htole64(fixed_channels));
    send_frame_callback_(conn, response_buffer);
  } else {
    SendCommandRejectNotUnderstood(conn, id);
  }
}

void FakeSignalingServer::SendCommandRejectNotUnderstood(hci::ConnectionHandle conn,
                                                         l2cap::CommandId id) {
  constexpr size_t kPayloadSize = sizeof(l2cap::CommandRejectPayload);
  constexpr size_t kResponseSize =
      sizeof(l2cap::CommandHeader) + sizeof(l2cap::CommandRejectPayload);
  StaticByteBuffer<kResponseSize> response_buffer;
  MutablePacketView<l2cap::CommandHeader> command_packet(&response_buffer, kPayloadSize);
  command_packet.mutable_header()->code = l2cap::kCommandRejectCode;
  command_packet.mutable_header()->id = id;
  command_packet.mutable_header()->length = htole16(kPayloadSize);
  auto* response_payload = command_packet.mutable_payload<l2cap::CommandRejectPayload>();
  response_payload->reason = static_cast<uint16_t>(l2cap::RejectReason::kNotUnderstood);
  send_frame_callback_(conn, response_buffer);
  return;
}

}  // namespace testing
}  // namespace bt
