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

#include <pw_bytes/endian.h>

#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {
bool LowEnergyCommandHandler::ConnectionParameterUpdateResponse::Decode(
    const ByteBuffer& payload_buf) {
  const uint16_t result = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little,
      static_cast<uint16_t>(payload_buf.ReadMember<&PayloadT::result>()));
  result_ = ConnectionParameterUpdateResult{result};
  return true;
}

bool LowEnergyCommandHandler::LeCreditBasedConnectionResponse::Decode(
    const ByteBuffer& payload_buf) {
  const uint16_t destination_cid = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little,
      static_cast<uint16_t>(payload_buf.ReadMember<&PayloadT::dst_cid>()));
  destination_cid_ = ChannelId{destination_cid};
  const uint16_t mtu = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little,
      static_cast<uint16_t>(payload_buf.ReadMember<&PayloadT::mtu>()));
  mtu_ = mtu;
  const uint16_t mps = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little,
      static_cast<uint16_t>(payload_buf.ReadMember<&PayloadT::mps>()));
  mps_ = mps;
  const uint16_t initial_credits = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little,
      static_cast<uint16_t>(
          payload_buf.ReadMember<&PayloadT::initial_credits>()));
  initial_credits_ = initial_credits;
  const uint16_t result = pw::bytes::ConvertOrderFrom(
      cpp20::endian::little,
      static_cast<uint16_t>(payload_buf.ReadMember<&PayloadT::result>()));
  result_ = LECreditBasedConnectionResult{result};
  return true;
}

LowEnergyCommandHandler::ConnectionParameterUpdateResponder::
    ConnectionParameterUpdateResponder(
        SignalingChannel::Responder* sig_responder)
    : Responder(sig_responder) {}

void LowEnergyCommandHandler::ConnectionParameterUpdateResponder::Send(
    ConnectionParameterUpdateResult result) {
  ConnectionParameterUpdateResponsePayload payload;
  payload.result = ConnectionParameterUpdateResult{pw::bytes::ConvertOrderTo(
      cpp20::endian::little, static_cast<uint16_t>(result))};
  sig_responder_->Send(BufferView(&payload, sizeof(payload)));
}

LowEnergyCommandHandler::LowEnergyCommandHandler(
    SignalingChannelInterface* sig, fit::closure request_fail_callback)
    : CommandHandler(sig, std::move(request_fail_callback)) {}

bool LowEnergyCommandHandler::SendLeCreditBasedConnectionRequest(
    uint16_t psm,
    uint16_t cid,
    uint16_t mtu,
    uint16_t mps,
    uint16_t credits,
    SendLeCreditBasedConnectionRequestCallback cb) {
  auto on_le_credit_based_connection_rsp =
      BuildResponseHandler<LeCreditBasedConnectionResponse>(std::move(cb));

  LECreditBasedConnectionRequestPayload payload;
  payload.le_psm = pw::bytes::ConvertOrderTo(cpp20::endian::little, psm);
  payload.src_cid = pw::bytes::ConvertOrderTo(cpp20::endian::little, cid);
  payload.mtu = pw::bytes::ConvertOrderTo(cpp20::endian::little, mtu);
  payload.mps = pw::bytes::ConvertOrderTo(cpp20::endian::little, mps);
  payload.initial_credits =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, credits);

  return sig()->SendRequest(kLECreditBasedConnectionRequest,
                            BufferView(&payload, sizeof(payload)),
                            std::move(on_le_credit_based_connection_rsp));
}

bool LowEnergyCommandHandler::SendConnectionParameterUpdateRequest(
    uint16_t interval_min,
    uint16_t interval_max,
    uint16_t peripheral_latency,
    uint16_t timeout_multiplier,
    ConnectionParameterUpdateResponseCallback cb) {
  auto on_param_update_rsp =
      BuildResponseHandler<ConnectionParameterUpdateResponse>(std::move(cb));

  ConnectionParameterUpdateRequestPayload payload;
  payload.interval_min =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, interval_min);
  payload.interval_max =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, interval_max);
  payload.peripheral_latency =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, peripheral_latency);
  payload.timeout_multiplier =
      pw::bytes::ConvertOrderTo(cpp20::endian::little, timeout_multiplier);

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
    const uint16_t interval_min =
        pw::bytes::ConvertOrderFrom(cpp20::endian::little, req.interval_min);
    const uint16_t interval_max =
        pw::bytes::ConvertOrderFrom(cpp20::endian::little, req.interval_max);
    const uint16_t peripheral_latency = pw::bytes::ConvertOrderFrom(
        cpp20::endian::little, req.peripheral_latency);
    const uint16_t timeout_multiplier = pw::bytes::ConvertOrderFrom(
        cpp20::endian::little, req.timeout_multiplier);
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
