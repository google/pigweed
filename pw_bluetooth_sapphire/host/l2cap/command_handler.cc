// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/l2cap/command_handler.h"

namespace bt::l2cap::internal {

bool CommandHandler::Response::ParseReject(const ByteBuffer& rej_payload_buf) {
  if (rej_payload_buf.size() < sizeof(CommandRejectPayload)) {
    bt_log(DEBUG,
           "l2cap",
           "cmd: ignoring malformed Command Reject, size %zu (expected >= %zu)",
           rej_payload_buf.size(),
           sizeof(CommandRejectPayload));
    return false;
  }
  reject_reason_ = static_cast<RejectReason>(
      le16toh(rej_payload_buf.ReadMember<&CommandRejectPayload::reason>()));

  if (reject_reason() == RejectReason::kInvalidCID) {
    if (rej_payload_buf.size() - sizeof(CommandRejectPayload) <
        sizeof(InvalidCIDPayload)) {
      bt_log(DEBUG,
             "l2cap",
             "cmd: ignoring malformed Command Reject Invalid Channel ID, size "
             "%zu (expected %zu)",
             rej_payload_buf.size(),
             sizeof(CommandRejectPayload) + sizeof(InvalidCIDPayload));
      return false;
    }
    const auto& invalid_cid_payload =
        rej_payload_buf.view(sizeof(CommandRejectPayload))
            .To<InvalidCIDPayload>();
    remote_cid_ = le16toh(invalid_cid_payload.src_cid);
    local_cid_ = le16toh(invalid_cid_payload.dst_cid);
  }

  return true;
}

bool CommandHandler::DisconnectionResponse::Decode(
    const ByteBuffer& payload_buf) {
  const auto disconn_rsp_payload = payload_buf.To<PayloadT>();
  local_cid_ = le16toh(disconn_rsp_payload.src_cid);
  remote_cid_ = le16toh(disconn_rsp_payload.dst_cid);
  return true;
}

CommandHandler::Responder::Responder(SignalingChannel::Responder* sig_responder,
                                     ChannelId local_cid,
                                     ChannelId remote_cid)
    : sig_responder_(sig_responder),
      local_cid_(local_cid),
      remote_cid_(remote_cid) {}

void CommandHandler::Responder::RejectNotUnderstood() {
  sig_responder_->RejectNotUnderstood();
}

void CommandHandler::Responder::RejectInvalidChannelId() {
  sig_responder_->RejectInvalidChannelId(local_cid(), remote_cid());
}

bool CommandHandler::SendDisconnectionRequest(
    ChannelId remote_cid,
    ChannelId local_cid,
    DisconnectionResponseCallback cb) {
  auto on_discon_rsp =
      BuildResponseHandler<DisconnectionResponse>(std::move(cb));

  DisconnectionRequestPayload payload = {htole16(remote_cid),
                                         htole16(local_cid)};
  return sig()->SendRequest(kDisconnectionRequest,
                            BufferView(&payload, sizeof(payload)),
                            std::move(on_discon_rsp));
}

void CommandHandler::ServeDisconnectionRequest(
    DisconnectionRequestCallback cb) {
  auto on_discon_req = [cb = std::move(cb)](
                           const ByteBuffer& request_payload,
                           SignalingChannel::Responder* sig_responder) {
    if (request_payload.size() != sizeof(DisconnectionRequestPayload)) {
      bt_log(DEBUG,
             "l2cap",
             "cmd: rejecting malformed Disconnection Request, size %zu",
             request_payload.size());
      sig_responder->RejectNotUnderstood();
      return;
    }

    const auto& discon_req = request_payload.To<DisconnectionRequestPayload>();
    const ChannelId local_cid = le16toh(discon_req.dst_cid);
    const ChannelId remote_cid = le16toh(discon_req.src_cid);
    DisconnectionResponder responder(sig_responder, local_cid, remote_cid);
    cb(local_cid, remote_cid, &responder);
  };

  sig()->ServeRequest(kDisconnectionRequest, std::move(on_discon_req));
}

CommandHandler::CommandHandler(SignalingChannelInterface* sig,
                               fit::closure request_fail_callback)
    : sig_(sig), request_fail_callback_(std::move(request_fail_callback)) {
  BT_ASSERT(sig_);
}

}  // namespace bt::l2cap::internal
