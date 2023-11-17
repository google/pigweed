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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/command_handler.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {
class LowEnergyCommandHandler final : public CommandHandler {
 public:
  class ConnectionParameterUpdateResponse final : public Response {
   public:
    using PayloadT = ConnectionParameterUpdateResponsePayload;
    static constexpr const char* kName = "Connection Parameter Update Response";

    using Response::Response;  // Inherit ctor
    bool Decode(const ByteBuffer& payload_buf);

    ConnectionParameterUpdateResult result() const { return result_; }

   private:
    friend class LowEnergyCommandHandler;

    ConnectionParameterUpdateResult result_;
  };

  class ConnectionParameterUpdateResponder final : public Responder {
   public:
    explicit ConnectionParameterUpdateResponder(
        SignalingChannel::Responder* sig_responder);

    void Send(ConnectionParameterUpdateResult result);
  };

  // |sig| must be valid for the lifetime of this object.
  // |command_failed_callback| is called if an outbound request timed out with
  // RTX or ERTX timers after retransmission (if configured). The call may come
  // after the lifetime of this object.
  explicit LowEnergyCommandHandler(
      SignalingChannelInterface* sig,
      fit::closure request_fail_callback = nullptr);
  ~LowEnergyCommandHandler() = default;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyCommandHandler);

  // Outbound request sending methods. Response callbacks are required to be
  // non-empty. The callbacks are wrapped and moved into the SignalingChannel
  // and may outlive LowEnergyCommandHandler.

  using ConnectionParameterUpdateResponseCallback =
      fit::function<void(const ConnectionParameterUpdateResponse& rsp)>;
  bool SendConnectionParameterUpdateRequest(
      uint16_t interval_min,
      uint16_t interval_max,
      uint16_t peripheral_latency,
      uint16_t timeout_multiplier,
      ConnectionParameterUpdateResponseCallback cb);

  // Inbound request delegate registration methods. The callbacks are wrapped
  // and moved into the SignalingChannel and may outlive
  // LowEnergyCommandHandler. It is expected that any request delegates
  // registered will span the lifetime of its signaling channel and hence link,
  // so no unregistration is provided. However each call to register will
  // replace any currently registered request delegate.

  using ConnectionParameterUpdateRequestCallback =
      fit::function<void(uint16_t interval_min,
                         uint16_t interval_max,
                         uint16_t peripheral_latency,
                         uint16_t timeout_multiplier,
                         ConnectionParameterUpdateResponder* responder)>;
  void ServeConnectionParameterUpdateRequest(
      ConnectionParameterUpdateRequestCallback cb);
};
}  // namespace bt::l2cap::internal
