// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/l2cap/bredr_signaling_channel.h"

#include "pw_bluetooth_sapphire/internal/host/common/log.h"

namespace bt::l2cap::internal {

BrEdrSignalingChannel::BrEdrSignalingChannel(
    Channel::WeakPtr chan,
    pw::bluetooth::emboss::ConnectionRole role,
    pw::async::Dispatcher& dispatcher)
    : SignalingChannel(std::move(chan), role, dispatcher) {
  set_mtu(kDefaultMTU);

  // Add default handler for incoming Echo Request commands.
  ServeRequest(kEchoRequest,
               [](const ByteBuffer& req_payload, Responder* responder) {
                 responder->Send(req_payload);
               });
}

bool BrEdrSignalingChannel::TestLink(const ByteBuffer& data, DataCallback cb) {
  return SendRequest(
      kEchoRequest,
      data,
      [cb = std::move(cb)](Status status, const ByteBuffer& rsp_payload) {
        if (status == Status::kSuccess) {
          cb(rsp_payload);
        } else {
          cb(BufferView());
        }
        return ResponseHandlerAction::kCompleteOutboundTransaction;
      });
}

void BrEdrSignalingChannel::DecodeRxUnit(ByteBufferPtr sdu,
                                         const SignalingPacketHandler& cb) {
  // "Multiple commands may be sent in a single C-frame over Fixed Channel CID
  // 0x0001 (ACL-U) (v5.0, Vol 3, Part A, Section 4)"
  BT_DEBUG_ASSERT(sdu);
  if (sdu->size() < sizeof(CommandHeader)) {
    bt_log(DEBUG, "l2cap-bredr", "sig: dropped malformed ACL signaling packet");
    return;
  }

  size_t sdu_offset = 0;
  while (sdu_offset + sizeof(CommandHeader) <= sdu->size()) {
    const auto header_data = sdu->view(sdu_offset, sizeof(CommandHeader));
    SignalingPacket packet(&header_data);

    uint16_t expected_payload_length = le16toh(packet.header().length);
    size_t remaining_sdu_length =
        sdu->size() - sdu_offset - sizeof(CommandHeader);
    if (remaining_sdu_length < expected_payload_length) {
      bt_log(DEBUG,
             "l2cap-bredr",
             "sig: expected more bytes (%zu < %u); drop",
             remaining_sdu_length,
             expected_payload_length);
      SendCommandReject(
          packet.header().id, RejectReason::kNotUnderstood, BufferView());
      return;
    }

    const auto packet_data =
        sdu->view(sdu_offset, sizeof(CommandHeader) + expected_payload_length);
    cb(SignalingPacket(&packet_data, expected_payload_length));

    sdu_offset += packet_data.size();
  }

  if (sdu_offset != sdu->size()) {
    bt_log(DEBUG,
           "l2cap-bredr",
           "sig: incomplete packet header "
           "(expected: %zu, left: %zu)",
           sizeof(CommandHeader),
           sdu->size() - sdu_offset);
  }
}

bool BrEdrSignalingChannel::IsSupportedResponse(CommandCode code) const {
  switch (code) {
    case kCommandRejectCode:
    case kConnectionResponse:
    case kConfigurationResponse:
    case kDisconnectionResponse:
    case kEchoResponse:
    case kInformationResponse:
      return true;
  }

  // Other response-type commands are for AMP/LE and are not supported.
  return false;
}

}  // namespace bt::l2cap::internal
