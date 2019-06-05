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
  BrEdrDynamicChannelRegistry(SignalingChannelInterface* sig,
                              DynamicChannelCallback close_cb,
                              ServiceRequestCallback service_request_cb);
  ~BrEdrDynamicChannelRegistry() override = default;

 private:
  // DynamicChannelRegistry override
  DynamicChannelPtr MakeOutbound(PSM psm, ChannelId local_cid) override;
  DynamicChannelPtr MakeInbound(PSM psm, ChannelId local_cid,
                                ChannelId remote_cid) override;

  // Signaling channel request handlers
  void OnRxConnReq(PSM psm, ChannelId remote_cid,
                   BrEdrCommandHandler::ConnectionResponder* responder);
  void OnRxConfigReq(ChannelId local_cid, uint16_t flags,
                     const ByteBuffer& options,
                     BrEdrCommandHandler::ConfigurationResponder* responder);
  void OnRxDisconReq(ChannelId local_cid, ChannelId remote_cid,
                     BrEdrCommandHandler::DisconnectionResponder* responder);
  void OnRxInfoReq(InformationType type,
                   BrEdrCommandHandler::InformationResponder* responder);

  SignalingChannelInterface* const sig_;
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
// Must be run only on the L2CAP thread.
class BrEdrDynamicChannel final : public DynamicChannel {
 public:
  static BrEdrDynamicChannelPtr MakeOutbound(
      DynamicChannelRegistry* registry,
      SignalingChannelInterface* signaling_channel, PSM psm,
      ChannelId local_cid);

  static BrEdrDynamicChannelPtr MakeInbound(
      DynamicChannelRegistry* registry,
      SignalingChannelInterface* signaling_channel, PSM psm,
      ChannelId local_cid, ChannelId remote_cid);

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

  // Inbound request handlers. Request must have a destination channel ID that
  // matches this instance's |local_cid|.
  void OnRxConfigReq(uint16_t flags, const ByteBuffer& options,
                     BrEdrCommandHandler::ConfigurationResponder* responder);
  void OnRxDisconReq(BrEdrCommandHandler::DisconnectionResponder* responder);

  // Reply with affirmative connection response and begin configuration.
  void CompleteInboundConnection(
      BrEdrCommandHandler::ConnectionResponder* responder);

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
                      SignalingChannelInterface* signaling_channel, PSM psm,
                      ChannelId local_cid, ChannelId remote_cid);

  // Deliver the result of channel connection and configuration to the |Open|
  // originator. Can be called multiple times but only the first invocation
  // passes the result.
  void PassOpenResult();

  // Error during channel connection or configuration (before it is open).
  // Deliver the error open result to the |Open| originator. Disconnect the
  // remote endpoint of the channel if possible. If the channel is already open,
  // use |Disconnect| instead.
  void PassOpenError();

  // Begin the local channel Configuration Request flow if it has not yet
  // happened.
  void TrySendLocalConfig();

  // Response handlers for outbound requests
  bool OnRxConnRsp(const BrEdrCommandHandler::ConnectionResponse& rsp);
  bool OnRxConfigRsp(const BrEdrCommandHandler::ConfigurationResponse& rsp);

  SignalingChannelInterface* const signaling_channel_;

  // Bit field assembled using the bit masks above. When zero, it represents a
  // closed (i.e. not yet open) channel.
  State state_;

  // This shall be reset to nullptr after invocation to enforce its single-use
  // semantics. See |DynamicChannel::Open| for details.
  fit::closure open_result_cb_;

  fxl::WeakPtrFactory<BrEdrDynamicChannel> weak_ptr_factory_;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_DYNAMIC_CHANNEL_H_
