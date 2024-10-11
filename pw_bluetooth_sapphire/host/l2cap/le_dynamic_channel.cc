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

#include "pw_bluetooth_sapphire/internal/host/l2cap/le_dynamic_channel.h"

#include <pw_bluetooth/l2cap_frames.emb.h>

#include <variant>

#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/low_energy_command_handler.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/types.h"

namespace bt::l2cap::internal {
namespace {

constexpr uint16_t kLeDynamicChannelCount =
    kLastLEDynamicChannelId - kFirstDynamicChannelId + 1;

CreditBasedFlowControlMode ConvertMode(AnyChannelMode mode) {
  // LE dynamic channels only support credit-based flow control modes.
  BT_ASSERT(std::holds_alternative<CreditBasedFlowControlMode>(mode));
  return std::get<CreditBasedFlowControlMode>(mode);
}

}  // namespace

LeDynamicChannelRegistry::LeDynamicChannelRegistry(
    SignalingChannelInterface* sig,
    DynamicChannelCallback close_cb,
    ServiceRequestCallback service_request_cb,
    bool random_channel_ids)
    : DynamicChannelRegistry(kLeDynamicChannelCount,
                             std::move(close_cb),
                             std::move(service_request_cb),
                             random_channel_ids),
      sig_(sig) {
  BT_DEBUG_ASSERT(sig_);
}

DynamicChannelPtr LeDynamicChannelRegistry::MakeOutbound(
    Psm psm, ChannelId local_cid, ChannelParameters params) {
  return LeDynamicChannel::MakeOutbound(this, sig_, psm, local_cid, params);
}

DynamicChannelPtr LeDynamicChannelRegistry::MakeInbound(
    [[maybe_unused]] Psm psm,
    [[maybe_unused]] ChannelId local_cid,
    [[maybe_unused]] ChannelId remote_cid,
    [[maybe_unused]] ChannelParameters params) {
  // Not yet implemented.
  return nullptr;
}

std::unique_ptr<LeDynamicChannel> LeDynamicChannel::MakeOutbound(
    DynamicChannelRegistry* registry,
    SignalingChannelInterface* signaling_channel,
    Psm psm,
    ChannelId local_cid,
    ChannelParameters params) {
  return std::unique_ptr<LeDynamicChannel>(new LeDynamicChannel(
      registry, signaling_channel, psm, local_cid, kInvalidChannelId, params));
}

std::string LeDynamicChannel::State::ToString() const {
  return std::string("{exchanged_connection_request: ") +
         (exchanged_connection_request ? "true" : "false") +
         ", exchanged_connection_response: " +
         (exchanged_connection_response ? "true" : "false") +
         ", exchanged_disconnect_request: " +
         (exchanged_disconnect_request ? "true" : "false") + "}";
}

LeDynamicChannel::LeDynamicChannel(DynamicChannelRegistry* registry,
                                   SignalingChannelInterface* signaling_channel,
                                   Psm psm,
                                   ChannelId local_cid,
                                   ChannelId remote_cid,
                                   ChannelParameters params)
    : DynamicChannel(registry, psm, local_cid, remote_cid),
      signaling_channel_(signaling_channel),
      flow_control_mode_(ConvertMode(params.mode.value_or(
          CreditBasedFlowControlMode::kLeCreditBasedFlowControl))),
      state_(),
      local_config_(
          LeChannelConfig{.mtu = params.max_rx_sdu_size.value_or(kDefaultMTU),
                          .mps = kMaxInboundPduPayloadSize}),
      remote_config_(std::nullopt),
      weak_self_(this) {}

void LeDynamicChannel::TriggerOpenCallback() {
  auto cb = std::move(open_result_cb_);
  if (cb) {
    cb();
  }
}

void LeDynamicChannel::Open(fit::closure open_cb) {
  BT_ASSERT_MSG(!open_result_cb_, "open callback already set");
  open_result_cb_ = std::move(open_cb);
  if (state_.exchanged_connection_request) {
    TriggerOpenCallback();
    return;
  }

  auto on_conn_rsp =
      [self = weak_self_.GetWeakPtr()](
          const LowEnergyCommandHandler::LeCreditBasedConnectionResponse&
              rsp) mutable {
        if (self.is_alive()) {
          self->OnRxLeCreditConnRsp(rsp);
          self->TriggerOpenCallback();
        }
      };

  auto on_conn_rsp_timeout = [cid = local_cid()] {
    bt_log(WARN,
           "l2cap-le",
           "Channel %#.4x: Timed out waiting for Connection Response",
           cid);
  };

  LowEnergyCommandHandler cmd_handler(signaling_channel_,
                                      std::move(on_conn_rsp_timeout));
  if (!cmd_handler.SendLeCreditBasedConnectionRequest(psm(),
                                                      local_cid(),
                                                      local_config_.mtu,
                                                      local_config_.mps,
                                                      0,
                                                      on_conn_rsp)) {
    bt_log(ERROR,
           "l2cap-le",
           "Channel %#.4x: Failed to send Connection Request",
           local_cid());
    TriggerOpenCallback();
    return;
  }

  state_.exchanged_connection_request = true;
}

void LeDynamicChannel::Disconnect(DisconnectDoneCallback done_cb) {
  BT_ASSERT(done_cb);
  if (!IsConnected()) {
    done_cb();
    return;
  }

  auto on_discon_rsp =
      [local_cid = local_cid(),
       remote_cid = remote_cid(),
       self = weak_self_.GetWeakPtr(),
       done_cb = done_cb.share()](
          const LowEnergyCommandHandler::DisconnectionResponse& rsp) mutable {
        if (rsp.local_cid() != local_cid || rsp.remote_cid() != remote_cid) {
          bt_log(WARN,
                 "l2cap-le",
                 "Channel %#.4x: Got Disconnection Response with ID %#.4x/"
                 "remote ID %#.4x on channel with remote ID %#.4x",
                 local_cid,
                 rsp.local_cid(),
                 rsp.remote_cid(),
                 remote_cid);
        } else {
          bt_log(TRACE,
                 "l2cap-le",
                 "Channel %#.4x: Got Disconnection Response",
                 local_cid);
        }

        if (self.is_alive()) {
          done_cb();
        }
      };

  auto on_discon_rsp_timeout = [local_cid = local_cid(),
                                self = weak_self_.GetWeakPtr(),
                                done_cb = done_cb.share()]() mutable {
    bt_log(WARN,
           "l2cap-le",
           "Channel %#.4x: Timed out waiting for Disconnection Response; "
           "completing disconnection",
           local_cid);
    if (self.is_alive()) {
      done_cb();
    }
  };

  LowEnergyCommandHandler cmd_handler(signaling_channel_,
                                      std::move(on_discon_rsp_timeout));
  if (!cmd_handler.SendDisconnectionRequest(
          remote_cid(), local_cid(), std::move(on_discon_rsp))) {
    bt_log(WARN,
           "l2cap-le",
           "Channel %#.4x: Failed to send Disconnection Request",
           local_cid());
    done_cb();
    return;
  }

  state_.exchanged_disconnect_request = true;
  bt_log(TRACE,
         "l2cap-le",
         "Channel %#.4x: Sent Disconnection Request",
         local_cid());
  return;
}

bool LeDynamicChannel::IsConnected() const {
  return state_.exchanged_connection_request &&
         state_.exchanged_connection_response &&
         !state_.exchanged_disconnect_request &&
         remote_cid() != kInvalidChannelId;
}

bool LeDynamicChannel::IsOpen() const {
  // Since dynamic LE L2CAP channels don't have channel configuration state
  // machines, `IsOpen` and `IsConnected` are equivalent.
  return IsConnected();
}

ChannelInfo LeDynamicChannel::info() const {
  BT_ASSERT(remote_config_.has_value());
  return ChannelInfo::MakeCreditBasedFlowControlMode(
      flow_control_mode_,
      local_config_.mtu,
      remote_config_->mtu,
      remote_config_->mps,
      remote_config_->initial_credits);
}

void LeDynamicChannel::OnRxLeCreditConnRsp(
    const LowEnergyCommandHandler::LeCreditBasedConnectionResponse& rsp) {
  if (state_.exchanged_connection_response ||
      !state_.exchanged_connection_request) {
    bt_log(ERROR,
           "l2cap-le",
           "Channel %#.4x: Unexpected Connection Response, state %s",
           local_cid(),
           bt_str(state_));
    return;
  }

  if (rsp.status() == LowEnergyCommandHandler::Status::kReject) {
    bt_log(ERROR,
           "l2cap-le",
           "Channel %#.4x: Connection Request rejected reason %#.4hx",
           local_cid(),
           static_cast<unsigned short>(rsp.reject_reason()));
    return;
  }

  if (rsp.result() != LECreditBasedConnectionResult::kSuccess) {
    bt_log(ERROR,
           "l2cap-le",
           "Channel %#.4x: Connection request failed, result %#.4hx",
           local_cid(),
           static_cast<uint16_t>(rsp.result()));
    return;
  }

  if (rsp.destination_cid() < kFirstDynamicChannelId) {
    bt_log(ERROR,
           "l2cap-le",
           "Channel %#.4x: Remote channel ID is invalid.",
           local_cid());
    return;
  }

  if (!SetRemoteChannelId(rsp.destination_cid())) {
    bt_log(ERROR,
           "l2cap-le",
           "Channel %#.4x: Remote channel ID %#.4x is not unique",
           local_cid(),
           rsp.destination_cid());
    return;
  }

  bt_log(TRACE,
         "l2cap-le",
         "Channel %#.4x: Got remote channel ID %#.4x",
         local_cid(),
         remote_cid());

  remote_config_ = LeChannelConfig{.mtu = rsp.mtu(),
                                   .mps = rsp.mps(),
                                   .initial_credits = rsp.initial_credits()};
  state_.exchanged_connection_response = true;
  set_opened();
}

}  // namespace bt::l2cap::internal
