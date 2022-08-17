// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_handler.h"

namespace bt::l2cap::internal {

bool CommandHandler::Response::ParseReject(const ByteBuffer& rej_payload_buf) {
  if (rej_payload_buf.size() < sizeof(CommandRejectPayload)) {
    bt_log(DEBUG, "l2cap", "cmd: ignoring malformed Command Reject, size %zu (expected >= %zu)",
           rej_payload_buf.size(), sizeof(CommandRejectPayload));
    return false;
  }
  reject_reason_ = static_cast<RejectReason>(
      letoh16(rej_payload_buf.ReadMember<&CommandRejectPayload::reason>()));

  if (reject_reason() == RejectReason::kInvalidCID) {
    if (rej_payload_buf.size() - sizeof(CommandRejectPayload) < sizeof(InvalidCIDPayload)) {
      bt_log(DEBUG, "l2cap",
             "cmd: ignoring malformed Command Reject Invalid Channel ID, size %zu (expected %zu)",
             rej_payload_buf.size(), sizeof(CommandRejectPayload) + sizeof(InvalidCIDPayload));
      return false;
    }
    const auto& invalid_cid_payload =
        rej_payload_buf.view(sizeof(CommandRejectPayload)).To<InvalidCIDPayload>();
    remote_cid_ = letoh16(invalid_cid_payload.src_cid);
    local_cid_ = letoh16(invalid_cid_payload.dst_cid);
  }

  return true;
}

bool CommandHandler::DisconnectionResponse::Decode(const ByteBuffer& payload_buf) {
  const auto disconn_rsp_payload = payload_buf.To<PayloadT>();
  local_cid_ = letoh16(disconn_rsp_payload.src_cid);
  remote_cid_ = letoh16(disconn_rsp_payload.dst_cid);
  return true;
}

CommandHandler::Responder::Responder(SignalingChannel::Responder* sig_responder,
                                     ChannelId local_cid, ChannelId remote_cid)
    : sig_responder_(sig_responder), local_cid_(local_cid), remote_cid_(remote_cid) {}

void CommandHandler::Responder::RejectNotUnderstood() { sig_responder_->RejectNotUnderstood(); }

void CommandHandler::Responder::RejectInvalidChannelId() {
  sig_responder_->RejectInvalidChannelId(local_cid(), remote_cid());
}

bool CommandHandler::SendDisconnectionRequest(ChannelId remote_cid, ChannelId local_cid,
                                              DisconnectionResponseCallback cb) {
  auto on_discon_rsp = BuildResponseHandler<DisconnectionResponse>(std::move(cb));

  DisconnectionRequestPayload payload = {htole16(remote_cid), htole16(local_cid)};
  return sig()->SendRequest(kDisconnectionRequest, BufferView(&payload, sizeof(payload)),
                            std::move(on_discon_rsp));
}

void CommandHandler::ServeDisconnectionRequest(DisconnectionRequestCallback cb) {
  auto on_discon_req = [cb = std::move(cb)](const ByteBuffer& request_payload,
                                            SignalingChannel::Responder* sig_responder) {
    if (request_payload.size() != sizeof(DisconnectionRequestPayload)) {
      bt_log(DEBUG, "l2cap", "cmd: rejecting malformed Disconnection Request, size %zu",
             request_payload.size());
      sig_responder->RejectNotUnderstood();
      return;
    }

    const auto& discon_req = request_payload.To<DisconnectionRequestPayload>();
    const ChannelId local_cid = letoh16(discon_req.dst_cid);
    const ChannelId remote_cid = letoh16(discon_req.src_cid);
    DisconnectionResponder responder(sig_responder, local_cid, remote_cid);
    cb(local_cid, remote_cid, &responder);
  };

  sig()->ServeRequest(kDisconnectionRequest, std::move(on_discon_req));
}

CommandHandler::CommandHandler(SignalingChannelInterface* sig, fit::closure request_fail_callback)
    : sig_(sig), request_fail_callback_(std::move(request_fail_callback)) {
  BT_ASSERT(sig_);
}

}  // namespace bt::l2cap::internal
