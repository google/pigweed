// Copyright 2024 The Pigweed Authors
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

#include <lib/fit/function.h>

#include <optional>
#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/l2cap/dynamic_channel_registry.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/low_energy_command_handler.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/signaling_channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/types.h"

namespace bt::l2cap::internal {

// Implements factories for LE dynamic channels and dispatches incoming
// signaling channel requests to the corresponding channels by local ID.
class LeDynamicChannelRegistry final : public DynamicChannelRegistry {
 public:
  LeDynamicChannelRegistry(SignalingChannelInterface* sig,
                           DynamicChannelCallback close_cb,
                           ServiceRequestCallback service_request_cb,
                           bool random_channel_ids);
  ~LeDynamicChannelRegistry() override = default;

 private:
  // DynamicChannelRegistry override
  DynamicChannelPtr MakeOutbound(Psm psm,
                                 ChannelId local_cid,
                                 ChannelParameters params) override;
  DynamicChannelPtr MakeInbound(Psm psm,
                                ChannelId local_cid,
                                ChannelId remote_cid,
                                ChannelParameters params) override;

  SignalingChannelInterface* const sig_;
};

struct LeChannelConfig {
  // Maximum length of an SDU that can be received.
  uint16_t mtu = 0;
  // Maximum length of a PDU payload that can be received.
  uint16_t mps = 0;
  // Initial credits, this is only set at channel creation time.
  uint16_t initial_credits = 0;
};

// Creates, configures, and tears down dynamic channels using the LE
// signaling channel. The lifetime of this object matches that of the channel
// itself: created in order to start an outbound channel or in response to an
// inbound channel request, then destroyed immediately after the channel is
// closed. This is intended to be created and owned by
// LeDynamicChannelRegistry.
class LeDynamicChannel final : public DynamicChannel {
 public:
  static std::unique_ptr<LeDynamicChannel> MakeOutbound(
      DynamicChannelRegistry* registry,
      SignalingChannelInterface* signaling_channel,
      Psm psm,
      ChannelId local_cid,
      ChannelParameters params);

  // DynamicChannel overrides
  ~LeDynamicChannel() override = default;

  void Open(fit::closure open_cb) override;
  void Disconnect(DisconnectDoneCallback done_cb) override;
  bool IsConnected() const override;
  bool IsOpen() const override;

  // Must not be called until channel is open.
  ChannelInfo info() const override;

  /// The setup state of an LE dynamic channel is much simpler than a BR/EDR
  /// channel, namely it does not have a configuration state machine. Instead,
  /// it is considered configured as soon as the
  /// L2CAP_(LE_)_CREDIT_BASED_CONNECTION_RSP is sent.
  struct State {
    // L2CAP_LE_CREDIT_BASED_CONNECTION_REQ or L2CAP_CREDIT_BASED_CONNECTION_REQ
    // transmitted in either direction.
    bool exchanged_connection_request = false;
    // L2CAP_LE_CREDIT_BASED_CONNECTION_RSP or L2CAP_CREDIT_BASED_CONNECTION_RSP
    // transmitted in opposite direction of REQ.
    bool exchanged_connection_response = false;
    // L2CAP_DISCONNECTION_REQ transmitted in either direction.
    bool exchanged_disconnect_request = false;

    // Produce a string representation of `State`.
    std::string ToString() const;
  };

 private:
  LeDynamicChannel(DynamicChannelRegistry* registry,
                   SignalingChannelInterface* signaling_channel,
                   Psm psm,
                   ChannelId local_cid,
                   ChannelId remote_cid,
                   ChannelParameters params);

  void TriggerOpenCallback();
  void OnRxLeCreditConnRsp(
      const LowEnergyCommandHandler::LeCreditBasedConnectionResponse& rsp);

  SignalingChannelInterface* const signaling_channel_;
  CreditBasedFlowControlMode const flow_control_mode_;

  State state_;
  LeChannelConfig local_config_;
  std::optional<LeChannelConfig> remote_config_;
  fit::closure open_result_cb_;
  WeakSelf<LeDynamicChannel> weak_self_;  // Keep last.
};

}  // namespace bt::l2cap::internal
