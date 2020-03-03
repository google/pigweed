// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "le_signaling_channel.h"

#include "channel.h"
#include "logical_link.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace l2cap {
namespace internal {

LESignalingChannel::LESignalingChannel(fbl::RefPtr<Channel> chan, hci::Connection::Role role)
    : SignalingChannel(std::move(chan), role) {
  set_mtu(kMinLEMTU);
}

bool LESignalingChannel::SendRequest(CommandCode req_code, const ByteBuffer& payload,
                                     ResponseHandler cb) {
  // TODO(NET-1093): Reuse BrEdrSignalingChannel's implementation.
  bt_log(WARN, "l2cap-le", "sig: SendRequest not implemented yet");
  return false;
}

void LESignalingChannel::ServeRequest(CommandCode req_code, RequestDelegate cb) {
  bt_log(WARN, "l2cap-le", "sig: ServeRequest not implemented yet");
}

void LESignalingChannel::OnConnParamUpdateReceived(const SignalingPacket& packet) {
  // Only a LE slave can send this command. "If an LE slave Host receives a
  // Connection Parameter Update Request packet it shall respond with a Command
  // Reject Packet [...]" (v5.0, Vol 3, Part A, Section 4.20).
  if (role() == hci::Connection::Role::kSlave) {
    bt_log(TRACE, "l2cap-le", "sig: rejecting conn. param. update request from master");
    SendCommandReject(packet.header().id, RejectReason::kNotUnderstood, BufferView());
    return;
  }

  if (packet.payload_size() != sizeof(ConnectionParameterUpdateRequestPayload)) {
    bt_log(TRACE, "l2cap-le", "sig: malformed request received");
    SendCommandReject(packet.header().id, RejectReason::kNotUnderstood, BufferView());
    return;
  }

  const auto& payload = packet.payload<ConnectionParameterUpdateRequestPayload>();

  // Reject the connection parameters if they are outside the ranges allowed by
  // the HCI specification (see HCI_LE_Connection_Update command - v5.0, Vol 2,
  // Part E, Section 7.8.18).
  bool reject = false;
  hci::LEPreferredConnectionParameters params(
      le16toh(payload.interval_min), le16toh(payload.interval_max), le16toh(payload.slave_latency),
      le16toh(payload.timeout_multiplier));

  if (params.min_interval() > params.max_interval()) {
    bt_log(TRACE, "l2cap-le", "sig: conn. min interval larger than max");
    reject = true;
  } else if (params.min_interval() < hci::kLEConnectionIntervalMin) {
    bt_log(TRACE, "l2cap-le", "sig: conn. min interval outside allowed range: %#.4x",
           params.min_interval());
    reject = true;
  } else if (params.max_interval() > hci::kLEConnectionIntervalMax) {
    bt_log(TRACE, "l2cap-le", "sig: conn. max interval outside allowed range: %#.4x",
           params.max_interval());
    reject = true;
  } else if (params.max_latency() > hci::kLEConnectionLatencyMax) {
    bt_log(TRACE, "l2cap-le", "sig: conn. slave latency too large: %#.4x", params.max_latency());
    reject = true;
  } else if (params.supervision_timeout() < hci::kLEConnectionSupervisionTimeoutMin ||
             params.supervision_timeout() > hci::kLEConnectionSupervisionTimeoutMax) {
    bt_log(TRACE, "l2cap-le", "sig: conn supv. timeout outside allowed range: %#.4x",
           params.supervision_timeout());
    reject = true;
  }

  ConnectionParameterUpdateResult result = reject ? ConnectionParameterUpdateResult::kRejected
                                                  : ConnectionParameterUpdateResult::kAccepted;
  ConnectionParameterUpdateResponsePayload rsp;
  rsp.result = static_cast<ConnectionParameterUpdateResult>(htole16(result));
  SendPacket(kConnectionParameterUpdateResponse, packet.header().id, BufferView(&rsp, sizeof(rsp)));

  if (!reject && dispatcher_) {
    async::PostTask(dispatcher_, [cb = conn_param_update_cb_.share(),
                                  params = std::move(params)]() mutable { cb(std::move(params)); });
  }
}

void LESignalingChannel::DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) {
  // "[O]nly one command per C-frame shall be sent over [the LE] Fixed Channel"
  // (v5.0, Vol 3, Part A, Section 4).
  ZX_DEBUG_ASSERT(sdu);
  if (sdu->size() < sizeof(CommandHeader)) {
    bt_log(TRACE, "l2cap-le", "sig: dropped malformed LE signaling packet");
    return;
  }

  SignalingPacket packet(sdu.get());
  uint16_t expected_payload_length = le16toh(packet.header().length);
  if (expected_payload_length != sdu->size() - sizeof(CommandHeader)) {
    bt_log(TRACE, "l2cap-le", "sig: packet size mismatch (expected: %u, recv: %zu); drop",
           expected_payload_length, sdu->size() - sizeof(CommandHeader));
    SendCommandReject(packet.header().id, RejectReason::kNotUnderstood, BufferView());
    return;
  }

  cb(SignalingPacket(sdu.get(), expected_payload_length));
}

bool LESignalingChannel::HandlePacket(const SignalingPacket& packet) {
  switch (packet.header().code) {
    case kConnectionParameterUpdateRequest:
      OnConnParamUpdateReceived(packet);
      return true;
    default:
      bt_log(TRACE, "l2cap-le", "sig: unsupported code %#.2x", packet.header().code);
      break;
  }

  return false;
}

bool LESignalingChannel::IsSupportedResponse(CommandCode code) const {
  switch (code) {
    case kCommandRejectCode:
    case kConnectionParameterUpdateResponse:
    case kDisconnectionResponse:
    case kLECreditBasedConnectionResponse:
      return true;
  }

  // Other response-type commands are for AMP/BREDR and are not supported.
  return false;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
