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

#include "pw_bluetooth_sapphire/internal/host/l2cap/low_energy_command_handler.h"

namespace bt::l2cap::internal {
bool LowEnergyCommandHandler::ConnectionParameterUpdateResponse::Decode(
    const ByteBuffer& payload_buf) {
  const auto result = le16toh(
      static_cast<uint16_t>(payload_buf.ReadMember<&PayloadT::result>()));
  result_ = ConnectionParameterUpdateResult{result};
  return true;
}

LowEnergyCommandHandler::ConnectionParameterUpdateResponder::
ConnectionParameterUpdateResponder(SignalingChannel::Responder* sig_responder)
    : Responder(sig_responder) {}

void LowEnergyCommandHandler::ConnectionParameterUpdateResponder::Send(
    ConnectionParameterUpdateResult result) {
  ConnectionParameterUpdateResponsePayload payload;
  payload.result =
      ConnectionParameterUpdateResult{htole16(static_cast<uint16_t>(result))};
  sig_responder_->Send(BufferView(&payload, sizeof(payload)));
}

LowEnergyCommandHandler::LowEnergyCommandHandler(
    SignalingChannelInterface* sig, fit::closure request_fail_callback)
    : CommandHandler(sig, std::move(request_fail_callback)) {}

bool LowEnergyCommandHandler::SendConnectionParameterUpdateRequest(
    uint16_t interval_min,
    uint16_t interval_max,
    uint16_t peripheral_latency,
    uint16_t timeout_multiplier,
    ConnectionParameterUpdateResponseCallback cb) {
  auto on_param_update_rsp =
      BuildResponseHandler<ConnectionParameterUpdateResponse>(std::move(cb));

  ConnectionParameterUpdateRequestPayload payload;
  payload.interval_min = htole16(interval_min);
  payload.interval_max = htole16(interval_max);
  payload.peripheral_latency = htole16(peripheral_latency);
  payload.timeout_multiplier = htole16(timeout_multiplier);

  return sig()->SendRequest(kConnectionParameterUpdateRequest,
                            BufferView(&payload, sizeof(payload)),
                            std::move(on_param_update_rsp));
}

void LowEnergyCommandHandler::ServeConnectionParameterUpdateRequest(
    ConnectionParameterUpdateRequestCallback cb) {
  auto on_param_update_req = [cb = std::move(cb)](
                                 const ByteBuffer& request_payload,
                                 SignalingChannel::Responder* sig_responder) {
    if (request_payload.size() !=
        sizeof(ConnectionParameterUpdateRequestPayload)) {
      bt_log(DEBUG,
             "l2cap-le",
             "cmd: rejecting malformed Connection Parameter Update Request, "
             "size %zu",
             request_payload.size());
      sig_responder->RejectNotUnderstood();
      return;
    }

    const auto& req =
        request_payload.To<ConnectionParameterUpdateRequestPayload>();
    const auto interval_min = le16toh(req.interval_min);
    const auto interval_max = le16toh(req.interval_max);
    const auto peripheral_latency = le16toh(req.peripheral_latency);
    const auto timeout_multiplier = le16toh(req.timeout_multiplier);
    ConnectionParameterUpdateResponder responder(sig_responder);
    cb(interval_min,
       interval_max,
       peripheral_latency,
       timeout_multiplier,
       &responder);
  };

  sig()->ServeRequest(kConnectionParameterUpdateRequest,
                      std::move(on_param_update_req));
}

}  // namespace bt::l2cap::internal
