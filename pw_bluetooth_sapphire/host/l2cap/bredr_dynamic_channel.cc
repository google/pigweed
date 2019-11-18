// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_dynamic_channel.h"

#include <endian.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_configuration.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {
namespace l2cap {
namespace internal {

BrEdrDynamicChannelRegistry::BrEdrDynamicChannelRegistry(SignalingChannelInterface* sig,
                                                         DynamicChannelCallback close_cb,
                                                         ServiceRequestCallback service_request_cb)
    : DynamicChannelRegistry(kLastACLDynamicChannelId, std::move(close_cb),
                             std::move(service_request_cb)),
      sig_(sig) {
  ZX_DEBUG_ASSERT(sig_);
  BrEdrCommandHandler cmd_handler(sig_);
  cmd_handler.ServeConnectionRequest(
      fit::bind_member(this, &BrEdrDynamicChannelRegistry::OnRxConnReq));
  cmd_handler.ServeConfigurationRequest(
      fit::bind_member(this, &BrEdrDynamicChannelRegistry::OnRxConfigReq));
  cmd_handler.ServeDisconnectionRequest(
      fit::bind_member(this, &BrEdrDynamicChannelRegistry::OnRxDisconReq));
  cmd_handler.ServeInformationRequest(
      fit::bind_member(this, &BrEdrDynamicChannelRegistry::OnRxInfoReq));
}

DynamicChannelPtr BrEdrDynamicChannelRegistry::MakeOutbound(PSM psm, ChannelId local_cid) {
  return BrEdrDynamicChannel::MakeOutbound(this, sig_, psm, local_cid);
}

DynamicChannelPtr BrEdrDynamicChannelRegistry::MakeInbound(PSM psm, ChannelId local_cid,
                                                           ChannelId remote_cid) {
  return BrEdrDynamicChannel::MakeInbound(this, sig_, psm, local_cid, remote_cid);
}

void BrEdrDynamicChannelRegistry::OnRxConnReq(PSM psm, ChannelId remote_cid,
                                              BrEdrCommandHandler::ConnectionResponder* responder) {
  bt_log(SPEW, "l2cap-bredr", "Got Connection Request for PSM %#.4x from channel %#.4x", psm,
         remote_cid);

  if (remote_cid == kInvalidChannelId) {
    bt_log(TRACE, "l2cap-bredr",
           "Invalid source CID; rejecting connection for PSM %#.4x from channel %#.4x", psm,
           remote_cid);
    responder->Send(kInvalidChannelId, ConnectionResult::kInvalidSourceCID,
                    ConnectionStatus::kNoInfoAvailable);
    return;
  }

  if (FindChannelByRemoteId(remote_cid) != nullptr) {
    bt_log(TRACE, "l2cap-bredr",
           "Remote CID already in use; rejecting connection for PSM %#.4x from channel %#.4x", psm,
           remote_cid);
    responder->Send(kInvalidChannelId, ConnectionResult::kSourceCIDAlreadyAllocated,
                    ConnectionStatus::kNoInfoAvailable);
    return;
  }

  ChannelId local_cid = FindAvailableChannelId();
  if (local_cid == kInvalidChannelId) {
    bt_log(TRACE, "l2cap-bredr",
           "Out of IDs; rejecting connection for PSM %#.4x from channel %#.4x", psm, remote_cid);
    responder->Send(kInvalidChannelId, ConnectionResult::kNoResources,
                    ConnectionStatus::kNoInfoAvailable);
    return;
  }

  auto dyn_chan = RequestService(psm, local_cid, remote_cid);
  if (!dyn_chan) {
    bt_log(TRACE, "l2cap-bredr",
           "Rejecting connection for unsupported PSM %#.4x from channel %#.4x", psm, remote_cid);
    responder->Send(kInvalidChannelId, ConnectionResult::kPSMNotSupported,
                    ConnectionStatus::kNoInfoAvailable);
    return;
  }

  static_cast<BrEdrDynamicChannel*>(dyn_chan)->CompleteInboundConnection(responder);
}

void BrEdrDynamicChannelRegistry::OnRxConfigReq(
    ChannelId local_cid, uint16_t flags, ChannelConfiguration config,
    BrEdrCommandHandler::ConfigurationResponder* responder) {
  auto channel = static_cast<BrEdrDynamicChannel*>(FindChannelByLocalId(local_cid));
  if (channel == nullptr) {
    bt_log(WARN, "l2cap-bredr", "ID %#.4x not found for Configuration Request", local_cid);
    responder->RejectInvalidChannelId();
    return;
  }

  channel->OnRxConfigReq(flags, std::move(config), responder);
}

void BrEdrDynamicChannelRegistry::OnRxDisconReq(
    ChannelId local_cid, ChannelId remote_cid,
    BrEdrCommandHandler::DisconnectionResponder* responder) {
  auto channel = static_cast<BrEdrDynamicChannel*>(FindChannelByLocalId(local_cid));
  if (channel == nullptr || channel->remote_cid() != remote_cid) {
    bt_log(WARN, "l2cap-bredr", "ID %#.4x not found for Disconnection Request (remote ID %#.4x)",
           local_cid, remote_cid);
    responder->RejectInvalidChannelId();
    return;
  }

  channel->OnRxDisconReq(responder);
}

void BrEdrDynamicChannelRegistry::OnRxInfoReq(
    InformationType type, BrEdrCommandHandler::InformationResponder* responder) {
  bt_log(SPEW, "l2cap-bredr", "Got Information Request for type %#.4hx", type);

  // TODO(NET-1074): The responses here will likely remain hardcoded magics, but
  // maybe they should live elsewhere.
  switch (type) {
    case InformationType::kConnectionlessMTU: {
      responder->SendNotSupported();
      break;
    }

    case InformationType::kExtendedFeaturesSupported: {
      const ExtendedFeatures extended_features = kExtendedFeaturesBitFixedChannels;

      // Express support for the Fixed Channel Supported feature
      responder->SendExtendedFeaturesSupported(extended_features);
      break;
    }

    case InformationType::kFixedChannelsSupported: {
      const FixedChannelsSupported channels_supported = kFixedChannelsSupportedBitSignaling;

      // Express support for the ACL-U Signaling Channel (as required)
      // TODO(NET-1074): Set the bit for SM's fixed channel
      responder->SendFixedChannelsSupported(channels_supported);
      break;
    }

    default:
      responder->RejectNotUnderstood();
      bt_log(TRACE, "l2cap-bredr", "Rejecting Information Request type %#.4hx", type);
  }
}

BrEdrDynamicChannelPtr BrEdrDynamicChannel::MakeOutbound(
    DynamicChannelRegistry* registry, SignalingChannelInterface* signaling_channel, PSM psm,
    ChannelId local_cid) {
  return std::unique_ptr<BrEdrDynamicChannel>(
      new BrEdrDynamicChannel(registry, signaling_channel, psm, local_cid, kInvalidChannelId));
}

BrEdrDynamicChannelPtr BrEdrDynamicChannel::MakeInbound(
    DynamicChannelRegistry* registry, SignalingChannelInterface* signaling_channel, PSM psm,
    ChannelId local_cid, ChannelId remote_cid) {
  auto channel = std::unique_ptr<BrEdrDynamicChannel>(
      new BrEdrDynamicChannel(registry, signaling_channel, psm, local_cid, remote_cid));
  channel->state_ |= kConnRequested;
  return channel;
}

void BrEdrDynamicChannel::Open(fit::closure open_result_cb) {
  open_result_cb_ = std::move(open_result_cb);

  if (state_ & kConnRequested) {
    return;
  }

  BrEdrCommandHandler cmd_handler(signaling_channel_);
  if (!cmd_handler.SendConnectionRequest(
          psm(), local_cid(), fit::bind_member(this, &BrEdrDynamicChannel::OnRxConnRsp))) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: Failed to send Connection Request", local_cid());
    PassOpenError();
    return;
  }

  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Sent Connection Request", local_cid());

  state_ |= kConnRequested;
}

void BrEdrDynamicChannel::Disconnect(DisconnectDoneCallback done_cb) {
  ZX_ASSERT(done_cb);
  if (!IsConnected()) {
    done_cb();
    return;
  }

  state_ |= kDisconnected;

  // Don't send disconnect request if the peer never responded (also can't,
  // because we don't have their end's ID).
  if (remote_cid() == kInvalidChannelId) {
    done_cb();
    return;
  }

  // TODO(BT-331): Call |done_cb| after RTX timeout to avoid zombie channels.
  auto on_discon_rsp =
      [local_cid = local_cid(), remote_cid = remote_cid(), self = weak_ptr_factory_.GetWeakPtr(),
       done_cb = done_cb.share()](const BrEdrCommandHandler::DisconnectionResponse& rsp) mutable {
        if (rsp.local_cid() != local_cid || rsp.remote_cid() != remote_cid) {
          bt_log(WARN, "l2cap-bredr",
                 "Channel %#.4x: Got Disconnection Response with ID %#.4x/"
                 "remote ID %#.4x on channel with remote ID %#.4x",
                 local_cid, rsp.local_cid(), rsp.remote_cid(), remote_cid);
        } else {
          bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Got Disconnection Response", local_cid);
        }

        if (self) {
          done_cb();
        }
      };

  BrEdrCommandHandler cmd_handler(signaling_channel_);
  if (!cmd_handler.SendDisconnectionRequest(remote_cid(), local_cid(), std::move(on_discon_rsp))) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: Failed to send Disconnection Request",
           local_cid());
    done_cb();
    return;
  }

  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Sent Disconnection Request", local_cid());
}

bool BrEdrDynamicChannel::IsConnected() const {
  // Remote-initiated channels have remote_cid_ already set.
  return (state_ & kConnRequested) && (state_ & kConnResponded) &&
         (remote_cid() != kInvalidChannelId) && !(state_ & kDisconnected);
}

bool BrEdrDynamicChannel::IsOpen() const {
  return IsConnected() && (state_ & kLocalConfigAccepted) && (state_ & kRemoteConfigAccepted);
}

BrEdrDynamicChannel::MtuConfiguration BrEdrDynamicChannel::mtu_configuration() const {
  ZX_DEBUG_ASSERT(IsOpen());
  ZX_DEBUG_ASSERT(local_config().mtu_option());
  ZX_DEBUG_ASSERT(remote_config().mtu_option());

  MtuConfiguration config;
  config.tx_mtu = remote_config().mtu_option()->mtu();
  config.rx_mtu = local_config().mtu_option()->mtu();
  return config;
}

void BrEdrDynamicChannel::OnRxConfigReq(uint16_t flags, ChannelConfiguration config,
                                        BrEdrCommandHandler::ConfigurationResponder* responder) {
  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Got Configuration Request (options: %s)", local_cid(),
         bt_str(config));

  if (!IsConnected()) {
    bt_log(WARN, "l2cap-bredr", "Channel %#.4x: Unexpected Configuration Request, state %x",
           local_cid(), state_);
    return;
  }

  if (state_ & kRemoteConfigReceived) {
    bt_log(TRACE, "l2cap-bredr", "Channel %#.4x: Reconfiguring, state %x", local_cid(), state_);
  }

  state_ |= kRemoteConfigReceived;

  // Reject request if it contains unknown options.
  // See Core Spec v5.1, Volume 3, Section 4.5: Configuration Options
  if (!config.unknown_options().empty()) {
    ChannelConfiguration::ConfigurationOptions unknown_options;
    std::string unknown_string;
    for (auto& option : config.unknown_options()) {
      unknown_options.push_back(std::make_unique<ChannelConfiguration::UnknownOption>(option));
      unknown_string += std::string(" ") + option.ToString();
    }

    bt_log(TRACE, "l2cap",
           "Channel %#.4x: config request contained unknown options (options: %s)\n", local_cid(),
           unknown_string.c_str());

    responder->Send(remote_cid(), 0x0000, ConfigurationResult::kUnknownOptions,
                    std::move(unknown_options));
    return;
  }

  // TODO(40053): reject reconfiguring MTU if mode is Enhanced Retransmission or Streaming mode.
  ChannelConfiguration::ConfigurationOptions unacceptable_options;

  // Reject MTUs below minimum size
  if (config.mtu_option() && config.mtu_option()->mtu() < kMinACLMTU) {
    bt_log(TRACE, "l2cap",
           "Channel %#.4x: config request contains MTU below minimum (mtu: %hu, min: %hu)",
           local_cid(), config.mtu_option()->mtu(), kMinACLMTU);
    // Respond back with a proposed MTU value of the required minimum (Core Spec v5.1, Vol 3, Part
    // A, Section 5.1)
    unacceptable_options.push_back(std::make_unique<ChannelConfiguration::MtuOption>(kMinACLMTU));
  }

  if (!unacceptable_options.empty()) {
    responder->Send(remote_cid(), 0x0000, ConfigurationResult::kUnacceptableParameters,
                    std::move(unacceptable_options));
    return;
  }

  // TODO(NET-1084): Defer accepting config req using a Pending response
  state_ |= kRemoteConfigAccepted;

  ChannelConfiguration response_config;

  // Successful response should include actual MTU local device will use. This must be min(received
  // MTU, local outgoing MTU capability). Currently, we accept any MTU.
  // TODO(41376): determine the upper bound of what we are actually capable of sending
  uint16_t actual_mtu = config.mtu_option() ? config.mtu_option()->mtu() : kDefaultMTU;
  response_config.set_mtu_option(ChannelConfiguration::MtuOption(actual_mtu));
  config.set_mtu_option(response_config.mtu_option());

  responder->Send(remote_cid(), 0x0000, ConfigurationResult::kSuccess, response_config.Options());

  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Sent Configuration Response (options: %s)",
         local_cid(), bt_str(response_config));

  // Save accepted options.
  remote_config_.Merge(config);

  if (IsOpen()) {
    set_opened();
    PassOpenResult();
  }
}

void BrEdrDynamicChannel::OnRxDisconReq(BrEdrCommandHandler::DisconnectionResponder* responder) {
  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Got Disconnection Request", local_cid());

  // Unconnected channels only exist if they are waiting for a Connection
  // Response from the peer or are disconnected yet undestroyed for some reason.
  // Getting a Disconnection Request implies some error condition or misbehavior
  // but the reaction should still be to terminate this channel.
  if (!IsConnected()) {
    bt_log(WARN, "l2cap-bredr", "Channel %#.4x: Unexpected Disconnection Request", local_cid());
  }

  state_ |= kDisconnected;
  responder->Send();
  if (opened()) {
    OnDisconnected();
  } else {
    PassOpenError();
  }
}

void BrEdrDynamicChannel::CompleteInboundConnection(
    BrEdrCommandHandler::ConnectionResponder* responder) {
  bt_log(TRACE, "l2cap-bredr", "Channel %#.4x: connected for PSM %#.4x from remote channel %#.4x",
         local_cid(), psm(), remote_cid());

  responder->Send(local_cid(), ConnectionResult::kSuccess, ConnectionStatus::kNoInfoAvailable);
  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Sent Connection Response", local_cid());
  state_ |= kConnResponded;
  TrySendLocalConfig();
}

BrEdrDynamicChannel::BrEdrDynamicChannel(DynamicChannelRegistry* registry,
                                         SignalingChannelInterface* signaling_channel, PSM psm,
                                         ChannelId local_cid, ChannelId remote_cid)
    : DynamicChannel(registry, psm, local_cid, remote_cid),
      signaling_channel_(signaling_channel),
      state_(0u),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(signaling_channel_);
  ZX_DEBUG_ASSERT(local_cid != kInvalidChannelId);

  local_config_.set_mtu_option(ChannelConfiguration::MtuOption(kMaxMTU));
}

void BrEdrDynamicChannel::PassOpenResult() {
  if (open_result_cb_) {
    // Guard against use-after-free if this object's owner destroys it while
    // running |open_result_cb_|.
    auto cb = std::move(open_result_cb_);
    cb();
  }
}

// This only checks that the channel had failed to open before passing to the
// client. The channel may still be connected, in case it's useful to perform
// channel configuration at this point.
void BrEdrDynamicChannel::PassOpenError() {
  ZX_ASSERT(!IsOpen());
  PassOpenResult();
}

void BrEdrDynamicChannel::TrySendLocalConfig() {
  if (state_ & kLocalConfigSent) {
    return;
  }

  BrEdrCommandHandler cmd_handler(signaling_channel_);
  if (!cmd_handler.SendConfigurationRequest(
          remote_cid(), 0, local_config_.Options(),
          [self = weak_ptr_factory_.GetWeakPtr()](auto& rsp) {
            if (self) {
              return self->OnRxConfigRsp(rsp);
            }
            return ResponseHandlerAction::kCompleteOutboundTransaction;
          })) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: Failed to send Configuration Request",
           local_cid());
    PassOpenError();
    return;
  }

  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Sent Configuration Request (options: %s)",
         local_cid(), bt_str(local_config_));

  state_ |= kLocalConfigSent;
}

BrEdrDynamicChannel::ResponseHandlerAction BrEdrDynamicChannel::OnRxConnRsp(
    const BrEdrCommandHandler::ConnectionResponse& rsp) {
  if (rsp.status() == BrEdrCommandHandler::Status::kReject) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: Connection Request rejected reason %#.4hx",
           local_cid(), rsp.reject_reason());
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  if (rsp.local_cid() != local_cid()) {
    bt_log(ERROR, "l2cap-bredr",
           "Channel %#.4x: Got Connection Response for another channel ID %#.4x", local_cid(),
           rsp.local_cid());
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  if ((state_ & kConnResponded) || !(state_ & kConnRequested)) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: Unexpected Connection Response, state %#x",
           local_cid(), state_);
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  if (rsp.result() == ConnectionResult::kPending) {
    bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Remote is pending open, status %#.4hx", local_cid(),
           rsp.conn_status());

    if (rsp.remote_cid() == kInvalidChannelId) {
      return ResponseHandlerAction::kExpectAdditionalResponse;
    }

    // If the remote provides a channel ID, then we store it. It can be used for
    // disconnection from this point forward.
    if (!SetRemoteChannelId(rsp.remote_cid())) {
      bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: Remote channel ID %#.4x is not unique",
             local_cid(), rsp.remote_cid());
      PassOpenError();
      return ResponseHandlerAction::kCompleteOutboundTransaction;
    }

    bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Got remote channel ID %#.4x", local_cid(),
           remote_cid());
    return ResponseHandlerAction::kExpectAdditionalResponse;
  }

  if (rsp.result() != ConnectionResult::kSuccess) {
    bt_log(ERROR, "l2cap-bredr",
           "Channel %#.4x: Unsuccessful Connection Response result %#.4hx, "
           "status %#.4x",
           local_cid(), rsp.result(), rsp.status());
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  if (rsp.remote_cid() < kFirstDynamicChannelId) {
    bt_log(ERROR, "l2cap-bredr",
           "Channel %#.4x: received Connection Response with invalid channel "
           "ID %#.4x, disconnecting",
           local_cid(), remote_cid());

    // This results in sending a Disconnection Request for non-zero remote IDs,
    // which is probably what we want because there's no other way to send back
    // a failure in this case.
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  // TODO(xow): To be stricter, we can disconnect if the remote ID changes on us
  // during connection like this, but not sure if that would be beneficial.
  if (remote_cid() != kInvalidChannelId && remote_cid() != rsp.remote_cid()) {
    bt_log(WARN, "l2cap-bredr",
           "Channel %#.4x: using new remote ID %#.4x after previous Connection "
           "Response provided %#.4x",
           local_cid(), rsp.remote_cid(), remote_cid());
  }

  state_ |= kConnResponded;

  if (!SetRemoteChannelId(rsp.remote_cid())) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: Remote channel ID %#.4x is not unique",
           local_cid(), rsp.remote_cid());
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Got remote channel ID %#.4x", local_cid(),
         rsp.remote_cid());

  TrySendLocalConfig();
  return ResponseHandlerAction::kCompleteOutboundTransaction;
}

BrEdrDynamicChannel::ResponseHandlerAction BrEdrDynamicChannel::OnRxConfigRsp(
    const BrEdrCommandHandler::ConfigurationResponse& rsp) {
  if (rsp.status() == BrEdrCommandHandler::Status::kReject) {
    bt_log(ERROR, "l2cap-bredr",
           "Channel %#.4x: Configuration Request rejected, reason %#.4hx, "
           "disconnecting",
           local_cid(), rsp.reject_reason());

    // Configuration Request being rejected is fatal because the remote is not
    // trying to negotiate parameters (any more).
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  // Pending responses may contain return values and adjustments to non-negotiated values.
  if (rsp.result() == ConfigurationResult::kPending) {
    bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: remote pending config (options: %s)", local_cid(),
           bt_str(rsp.config()));

    if (rsp.config().mtu_option()) {
      local_config_.set_mtu_option(rsp.config().mtu_option());
    }

    return ResponseHandlerAction::kExpectAdditionalResponse;
  }

  if (rsp.result() != ConfigurationResult::kSuccess) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: unsuccessful config (result: %#.4hx, options: %s)",
           local_cid(), rsp.result(), bt_str(rsp.config()));
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  if (rsp.local_cid() != local_cid()) {
    bt_log(ERROR, "l2cap-bredr", "Channel %#.4x: dropping Configuration Response for %#.4x",
           local_cid(), rsp.local_cid());
    PassOpenError();
    return ResponseHandlerAction::kCompleteOutboundTransaction;
  }

  state_ |= kLocalConfigAccepted;

  bt_log(SPEW, "l2cap-bredr", "Channel %#.4x: Got Configuration Response (options: %s)",
         local_cid(), bt_str(rsp.config()));

  if (rsp.config().mtu_option()) {
    local_config_.set_mtu_option(rsp.config().mtu_option());
  }

  if (IsOpen()) {
    set_opened();
    PassOpenResult();
  }

  return ResponseHandlerAction::kCompleteOutboundTransaction;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
