// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_DYNAMIC_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_DYNAMIC_CHANNEL_H_

#include <lib/fit/function.h>

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_command_handler.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/dynamic_channel_registry.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/signaling_channel.h"

namespace bt {
namespace l2cap {
namespace internal {

// Implements factories for BR/EDR dynamic channels and dispatches incoming
// signaling channel requests to the corresponding channels by local ID.
//
// Must be run only on the L2CAP thread.
class BrEdrDynamicChannelRegistry final : public DynamicChannelRegistry {
 public:
  BrEdrDynamicChannelRegistry(SignalingChannelInterface* sig, DynamicChannelCallback close_cb,
                              ServiceRequestCallback service_request_cb);
  ~BrEdrDynamicChannelRegistry() override = default;

  std::optional<ExtendedFeatures> extended_features() { return extended_features_; };

 private:
  // DynamicChannelRegistry override
  DynamicChannelPtr MakeOutbound(PSM psm, ChannelId local_cid, ChannelParameters params) override;
  DynamicChannelPtr MakeInbound(PSM psm, ChannelId local_cid, ChannelId remote_cid,
                                ChannelParameters params) override;

  // Signaling channel request handlers
  void OnRxConnReq(PSM psm, ChannelId remote_cid,
                   BrEdrCommandHandler::ConnectionResponder* responder);
  void OnRxConfigReq(ChannelId local_cid, uint16_t flags, ChannelConfiguration config,
                     BrEdrCommandHandler::ConfigurationResponder* responder);
  void OnRxDisconReq(ChannelId local_cid, ChannelId remote_cid,
                     BrEdrCommandHandler::DisconnectionResponder* responder);
  void OnRxInfoReq(InformationType type, BrEdrCommandHandler::InformationResponder* responder);

  // Signaling channel response handlers
  void OnRxExtendedFeaturesInfoRsp(const BrEdrCommandHandler::InformationResponse& rsp);

  // Send extended features information request.
  // TODO(929): Send fixed channels information request.
  void SendInformationRequests();

  // If an extended features information response has been received, returns the value of the ERTM
  // bit in the peer's feature mask.
  std::optional<bool> PeerSupportsERTM() const;

  using State = uint8_t;
  enum StateBit : State {
    // Extended Features Information Request (transmitted from local to remote)
    kExtendedFeaturesSent = (1 << 0),

    // Extended Features Information Response (transmitted from remote to local)
    kExtendedFeaturesReceived = (1 << 1),
  };

  // Bit field assembled using the bit masks above.
  State state_;

  SignalingChannelInterface* const sig_;

  std::optional<ExtendedFeatures> extended_features_;
};

class BrEdrDynamicChannel;
using BrEdrDynamicChannelPtr = std::unique_ptr<BrEdrDynamicChannel>;

// Creates, configures, and tears down dynamic channels using the BR/EDR
// signaling channel. The lifetime of this object matches that of the channel
// itself: created in order to start an outbound channel or in response to an
// inbound channel request, then destroyed immediately after the channel is
// closed. This is intended to be created and owned by
// BrEdrDynamicChannelRegistry.
//
// This implements the state machine described by v5.0 Vol 3 Part A Sec 6. The
// state of OPEN ("user data transfer state") matches the implementation of the
// |DynamicChannel::IsOpen()|.
//
// Channel Configuration design:
// Implementation-defined behavior:
// * Inbound and outbound configuration requests/responses are exchanged simultaneously
// * If the desired channel mode is ERTM, the configuration request is not sent until the extended
//   features mask is received from the peer.
//   * If the peer doesn't support ERTM, the local device will negotiate Basic Mode instead of ERTM
// * after both configuration requests have been accepted, if they are inconsistent, the channel
//   is disconnected (this can happen if the peer doesn't follow the spec)
// Configuration Request Handling:
// * when the peer requests an MTU below the minumum, send an Unacceptable Parameters response
//     suggesting the minimum MTU.
// * allow peer to send a maximum of 2 configuration requests with undesired channel modes
//   before disconnecting
// * reject all channel modes other than Basic Mode and ERTM
// Negative Configuration Response Handling:
// A maximum of 2 negotiation attempts will be made before disconnecting, according to the
// following rules:
// * if the response does not contain the Retransmission & Flow Control option, disconnect
// * when the response specifies a different channel mode than the peer sent in a configuration
//   request, disconnect
// * when the response rejected Basic Mode, disconnect
// * otherwise, send a second configuration request with Basic Mode
//
// Must be run only on the L2CAP thread.
class BrEdrDynamicChannel final : public DynamicChannel {
 public:
  using ResponseHandlerAction = SignalingChannel::ResponseHandlerAction;

  static BrEdrDynamicChannelPtr MakeOutbound(DynamicChannelRegistry* registry,
                                             SignalingChannelInterface* signaling_channel, PSM psm,
                                             ChannelId local_cid, ChannelParameters params,
                                             std::optional<bool> peer_supports_ertm);

  static BrEdrDynamicChannelPtr MakeInbound(DynamicChannelRegistry* registry,
                                            SignalingChannelInterface* signaling_channel, PSM psm,
                                            ChannelId local_cid, ChannelId remote_cid,
                                            ChannelParameters params,
                                            std::optional<bool> peer_supports_ertm);

  // DynamicChannel overrides
  ~BrEdrDynamicChannel() override = default;

  void Open(fit::closure open_cb) override;

  // Mark this channel as closed and disconnected. Send a Disconnection Request
  // to the peer if possible (peer had sent an ID for its endpoint). |done_cb|
  // will be called when Disconnection Response is received or if channel is
  // already not connected.
  void Disconnect(DisconnectDoneCallback done_cb) override;

  bool IsConnected() const override;
  bool IsOpen() const override;

  // Current high-level channel configuration parameters. Must not be called until channel is open.
  ChannelParameters parameters() const override;

  // Must not be called until channel is open.
  MtuConfiguration mtu_configuration() const override;

  // Inbound request handlers. Request must have a destination channel ID that
  // matches this instance's |local_cid|.
  void OnRxConfigReq(uint16_t flags, ChannelConfiguration config,
                     BrEdrCommandHandler::ConfigurationResponder* responder);
  void OnRxDisconReq(BrEdrCommandHandler::DisconnectionResponder* responder);

  // Called when the peer indicates whether it supports Enhanced Retransmission Mode.
  // Kicks off the configuration process if the preferred channel mode is ERTM.
  void SetEnhancedRetransmissionSupport(bool supported);

  // Reply with affirmative connection response and begin configuration.
  void CompleteInboundConnection(BrEdrCommandHandler::ConnectionResponder* responder);

  // Contains options configured by remote configuration requests (Core Spec v5.1, Vol 3, Part A,
  // Sections 5 and 7.1.1).
  const ChannelConfiguration& remote_config() const { return remote_config_; }

  // Contains options configured by local configuration requests (Core Spec v5.1, Vol 3, Part A,
  // Sections 5 and 7.1.2).
  const ChannelConfiguration& local_config() const { return local_config_; }

 private:
  // The channel configuration state is described in v5.0 Vol 3 Part A Sec 6 (in
  // particular in Fig. 6.2) as having numerous substates in order to capture
  // the different orders in which configuration packets may be transmitted. It
  // is implemented here as a bitfield where bits are set as each packet is
  // transmitted.
  //
  // The initial state for a channel prior to any signaling packets transmitted
  // is 0.
  using State = uint8_t;
  enum StateBit : State {
    // Connection Req (transmitted in either direction)
    kConnRequested = (1 << 0),

    // Connection Rsp (transmitted in opposite direction of Connection Req)
    kConnResponded = (1 << 1),

    // Configuration Req (transmitted from local to remote)
    kLocalConfigSent = (1 << 2),

    // Configuration Rsp (successful; transmitted from remote to local)
    kLocalConfigAccepted = (1 << 3),

    // Configuration Req (transmitted from remote to local)
    kRemoteConfigReceived = (1 << 4),

    // Configuration Rsp (successful; transmitted from local to remote)
    kRemoteConfigAccepted = (1 << 5),

    // Disconnection Req (transmitted in either direction)
    kDisconnected = (1 << 6),
  };

  // TODO(NET-1319): Add Extended Flow Specification steps (exchange &
  // controller configuration)

  BrEdrDynamicChannel(DynamicChannelRegistry* registry,
                      SignalingChannelInterface* signaling_channel, PSM psm, ChannelId local_cid,
                      ChannelId remote_cid, ChannelParameters params,
                      std::optional<bool> peer_supports_ertm);

  // Deliver the result of channel connection and configuration to the |Open|
  // originator. Can be called multiple times but only the first invocation
  // passes the result.
  void PassOpenResult();

  // Error during channel connection or configuration (before it is open).
  // Deliver the error open result to the |Open| originator. Disconnect the
  // remote endpoint of the channel if possible. If the channel is already open,
  // use |Disconnect| instead.
  void PassOpenError();

  // The local configuration channel mode may need to be changed from ERTM to Basic Mode if the peer
  // does not support ERTM. This channel must not be waiting for extended features when this method
  // is called. No-op if extended features have not been received yet (e.g. when a basic mode
  // channel configuration flow is initiated before extended features have been received).
  void UpdateLocalConfigForErtm();

  // Returns true if the preferred channel parameters require waiting for an extended features
  // information response and the response has not yet been received. Must be false before sending
  // local config.
  bool IsWaitingForPeerErtmSupport();

  // Begin the local channel Configuration Request flow if it has not yet
  // happened. The channel must not be waiting for the extended features info response.
  void TrySendLocalConfig();

  // Send local configuration request.
  void SendLocalConfig();

  // Returns true if both the remote and local configs have been accepted.
  bool BothConfigsAccepted() const;

  // Returns true if negotiated channel modes are consistent. Must not be called until after both
  // configs have been accepted (|BothConfigsAccepted()| is true).
  [[nodiscard]] bool AcceptedChannelModesAreConsistent() const;

  // Checks options in a configuration request for unacceptable MTU and Retransmission and Flow
  // Control options. Returns a configuration object where for each unacceptable option, there
  // is a corresponding option with a value that would have been accepted if sent in the
  // original request.
  [[nodiscard]] ChannelConfiguration CheckForUnacceptableConfigReqOptions(
      const ChannelConfiguration& config);

  // Try to recover from a configuration response with the "Unacceptable Parameters" result.
  // Returns true if the negative reponse could be recovered from, and false otherwise (in which
  // case an error should be reported).
  [[nodiscard]] bool TryRecoverFromUnacceptableParametersConfigRsp(
      const ChannelConfiguration& config);

  // Response handlers for outbound requests
  ResponseHandlerAction OnRxConnRsp(const BrEdrCommandHandler::ConnectionResponse& rsp);
  ResponseHandlerAction OnRxConfigRsp(const BrEdrCommandHandler::ConfigurationResponse& rsp);

  SignalingChannelInterface* const signaling_channel_;

  // Bit field assembled using the bit masks above. When zero, it represents a
  // closed (i.e. not yet open) channel.
  State state_;

  // This shall be reset to nullptr after invocation to enforce its single-use
  // semantics. See |DynamicChannel::Open| for details.
  fit::closure open_result_cb_;

  // Support for ERTM is indicated in the peer's extended features mask, received in the extended
  // features information response. Since the response may not yet have been received when this
  // channel is created, this value may be assigned in either the constructor or in the
  // |SetEnhancedRetransmissionSupport| callback.
  std::optional<bool> peer_supports_ertm_;

  // Contains options configured by remote configuration requests (Core Spec v5.1, Vol 3, Part A,
  // Sections 5 and 7.1.1).
  ChannelConfiguration remote_config_;

  // Contains options configured by local configuration requests (Core Spec v5.1, Vol 3, Part A,
  // Sections 5 and 7.1.2).
  ChannelConfiguration local_config_;

  fxl::WeakPtrFactory<BrEdrDynamicChannel> weak_ptr_factory_;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_DYNAMIC_CHANNEL_H_
