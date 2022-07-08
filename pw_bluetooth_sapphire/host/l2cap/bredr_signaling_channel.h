// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_SIGNALING_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_SIGNALING_CHANNEL_H_

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/signaling_channel.h"

namespace bt::l2cap::internal {

// Implements packet processing for the BR/EDR signaling channel (CID = 1).
// Callbacks will be run on the thread where packet reception occurs, which is
// the L2CAP thread in production.
class BrEdrSignalingChannel final : public SignalingChannel {
 public:
  BrEdrSignalingChannel(fxl::WeakPtr<Channel> chan, hci_spec::ConnectionRole role);
  ~BrEdrSignalingChannel() override = default;

  // Test the link using an Echo Request command that can have an arbitrary
  // payload. The callback will be invoked with the remote's Echo Response
  // payload (if any) on the L2CAP thread, or with an empty buffer if the
  // remote responded with a rejection. Returns false if the request failed to
  // send.
  //
  // This is implemented as v5.0 Vol 3, Part A Section 4.8: "These requests may be
  // used for testing the link or for passing vendor specific information using
  // the optional data field."
  bool TestLink(const ByteBuffer& data, DataCallback cb);

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) override;
  bool IsSupportedResponse(CommandCode code) const override;
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_BREDR_SIGNALING_CHANNEL_H_
