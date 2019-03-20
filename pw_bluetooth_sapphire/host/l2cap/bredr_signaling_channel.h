// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_SIGNALING_CHANNEL_H_

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/signaling_channel.h"

namespace bt {
namespace l2cap {
namespace internal {

// Implements packet processing for the BR/EDR signaling channel (CID = 1).
// Callbacks will be run on the thread where packet reception occurs, which is
// the L2CAP thread in production.
class BrEdrSignalingChannel final : public SignalingChannel {
 public:
  BrEdrSignalingChannel(fbl::RefPtr<Channel> chan, hci::Connection::Role role);
  ~BrEdrSignalingChannel() override = default;

  // SignalingChannelInterface overrides
  // TODO(NET-1093): Refactor implementation into SignalingChannel so it's
  // shared with LESignalingChannel.
  bool SendRequest(CommandCode req_code, const common::ByteBuffer& payload,
                   ResponseHandler cb) override;
  void ServeRequest(CommandCode req_code, RequestDelegate cb) override;

  // Test the link using an Echo Request command that can have an arbitrary
  // payload. The callback will be invoked with the remote's Echo Response
  // payload (if any) on the L2CAP thread, or with an empty buffer if the
  // remote responded with a rejection. Returns false if the request failed to
  // send.
  bool TestLink(const common::ByteBuffer& data, DataCallback cb);

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(common::ByteBufferPtr sdu,
                    const SignalingPacketHandler& cb) override;
  bool HandlePacket(const SignalingPacket& packet) override;

  // Register a callback that will be invoked when a response-type command
  // packet (specified by |expected_code|) is received. Returns the identifier
  // to be included in the header of the outgoing request packet (or
  // kInvalidCommandId if all valid command identifiers are pending responses).
  // If the signaling channel receives a Command Reject that matches the same
  // identifier, the rejection packet will be forwarded to the callback instead.
  // |handler| will be run on the L2CAP thread.
  //
  // TODO(xow): Add function to cancel a queued response.
  CommandId EnqueueResponse(CommandCode expected_code, ResponseHandler cb);

  // True if the code is for a supported ACL-U response-type signaling command.
  bool IsSupportedResponse(CommandCode code) const;

  // True if an outbound request-type command has registered a callback for its
  // response matching a particular |id|.
  bool IsCommandPending(CommandId id) const;

  // Called when a response-type command packet is received. Sends a Command
  // Reject if no ResponseHandler was registered for inbound packet's command
  // code and identifier.
  void OnRxResponse(const SignalingPacket& packet);

  // Stores response handlers for requests that have been sent.
  std::unordered_map<CommandId, std::pair<CommandCode, ResponseHandler>>
      pending_commands_;

  // Stores handlers for incoming request packets.
  std::unordered_map<CommandCode, RequestDelegate> inbound_handlers_;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_SIGNALING_CHANNEL_H_
