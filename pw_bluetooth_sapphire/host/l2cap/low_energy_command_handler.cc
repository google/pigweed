// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/low_energy_command_handler.h"

namespace bt::l2cap::internal {
bool LowEnergyCommandHandler::ConnectionParameterUpdateResponse::Decode(
    const ByteBuffer& payload_buf) {
  auto& payload = payload_buf.As<PayloadT>();
  result_ = ConnectionParameterUpdateResult{letoh16(payload.result)};
  return true;
}

LowEnergyCommandHandler::ConnectionParameterUpdateResponder::ConnectionParameterUpdateResponder(
    SignalingChannel::Responder* sig_responder)
    : Responder(sig_responder) {}

void LowEnergyCommandHandler::ConnectionParameterUpdateResponder::Send(
    ConnectionParameterUpdateResult result) {
  ConnectionParameterUpdateResponsePayload payload;
  payload.result = ConnectionParameterUpdateResult{htole16(result)};
  sig_responder_->Send(BufferView(&payload, sizeof(payload)));
}

LowEnergyCommandHandler::LowEnergyCommandHandler(SignalingChannelInterface* sig,
                                                 fit::closure request_fail_callback)
    : CommandHandler(sig, std::move(request_fail_callback)) {}

bool LowEnergyCommandHandler::SendConnectionParameterUpdateRequest(
    uint16_t interval_min, uint16_t interval_max, uint16_t slave_latency,
    uint16_t timeout_multiplier, ConnectionParameterUpdateResponseCallback cb) {
  auto on_param_update_rsp = BuildResponseHandler<ConnectionParameterUpdateResponse>(std::move(cb));

  ConnectionParameterUpdateRequestPayload payload;
  payload.interval_min = htole16(interval_min);
  payload.interval_max = htole16(interval_max);
  payload.slave_latency = htole16(slave_latency);
  payload.timeout_multiplier = htole16(timeout_multiplier);

  return sig()->SendRequest(kConnectionParameterUpdateRequest,
                            BufferView(&payload, sizeof(payload)), std::move(on_param_update_rsp));
}

void LowEnergyCommandHandler::ServeConnectionParameterUpdateRequest(
    ConnectionParameterUpdateRequestCallback cb) {
  auto on_param_update_req = [cb = std::move(cb)](const ByteBuffer& request_payload,
                                                  SignalingChannel::Responder* sig_responder) {
    if (request_payload.size() != sizeof(ConnectionParameterUpdateRequestPayload)) {
      bt_log(TRACE, "l2cap-le",
             "cmd: rejecting malformed Connection Parameter Update Request, size %zu",
             request_payload.size());
      sig_responder->RejectNotUnderstood();
      return;
    }

    const auto& req = request_payload.As<ConnectionParameterUpdateRequestPayload>();
    const auto interval_min = letoh16(req.interval_min);
    const auto interval_max = letoh16(req.interval_max);
    const auto slave_latency = letoh16(req.slave_latency);
    const auto timeout_multiplier = letoh16(req.timeout_multiplier);
    ConnectionParameterUpdateResponder responder(sig_responder);
    cb(interval_min, interval_max, slave_latency, timeout_multiplier, &responder);
  };

  sig()->ServeRequest(kConnectionParameterUpdateRequest, std::move(on_param_update_req));
}

}  // namespace bt::l2cap::internal
